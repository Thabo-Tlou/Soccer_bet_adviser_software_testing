"""
EPL match-outcome model (Home win / Draw / Away win) with draw-aware decisions.

Why the original never predicted draws
--------------------------------------
A draw almost never has the highest probability in a single match (it is
usually ~25-30%), so `argmax(probabilities)` essentially never chooses it. That
is not a bug in the probabilities, it is a flaw in the decision rule. This
version separates the two jobs:

  1. Probability quality  -> RF (calibrated) + Poisson (draw-inflated) ensemble
  2. Hard predictions     -> class-weight decision rule tuned on a validation
                             window so that draws are actually called when the
                             draw probability is high relative to its usual level.

Chronological layout (no leakage):
    train (70%)  ->  validation (15%, used ONLY for tuning)  ->  test (15%)
After tuning, models are refit on train+validation and scored once on test.
"""

import json
import warnings
from collections import defaultdict
from pathlib import Path

import numpy as np
import pandas as pd
from scipy.ndimage import uniform_filter
from scipy.stats import poisson
from sklearn.calibration import CalibratedClassifierCV
from sklearn.ensemble import RandomForestClassifier
from sklearn.impute import SimpleImputer
from sklearn.linear_model import PoissonRegressor
from sklearn.metrics import (
    accuracy_score,
    balanced_accuracy_score,
    classification_report,
    confusion_matrix,
    f1_score,
    log_loss,
)
from sklearn.model_selection import TimeSeriesSplit
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler

warnings.filterwarnings("ignore", category=UserWarning)
warnings.filterwarnings("ignore", category=FutureWarning)

# ----------------------------------------------------------------------------
# Configuration
# ----------------------------------------------------------------------------
DATA_PATHS = [
    Path("data/EPL_merged_2017-2027.csv"),
    Path("EPL_merged_2017-2027.csv"),
]
OUTPUT_DIR = Path("models")
MODEL_PATH = OUTPUT_DIR / "random_forest_epl.onnx"
IMPORTANCE_PATH = OUTPUT_DIR / "epl_feature_importance.csv"
RESULTS_PATH = OUTPUT_DIR / "epl_test_predictions.csv"
BACKTEST_PATH = OUTPUT_DIR / "epl_betting_backtest.csv"
CONFIG_PATH = OUTPUT_DIR / "epl_decision_config.json"

VAL_SIZE = 0.15
TEST_SIZE = 0.15
RANDOM_STATE = 42

# Elo
INITIAL_ELO = 1500.0
PROMOTED_ELO = 1430.0       # teams first seen after the first season
SEASON_REGRESSION = 0.25    # share of rating pulled back to 1500 each new season
ELO_K = 24.0
HOME_ADVANTAGE = 60.0

# Features
USE_MARKET_FEATURES = True  # bookmaker-implied probabilities as inputs
MAX_MISSING_RATE = 0.50     # drop features that are mostly empty (e.g. xG)

# Hard-prediction rule: "macro_f1" (balances all 3 classes) or "balanced_accuracy"
DECISION_OBJECTIVE = "macro_f1"

# Betting backtest
MIN_EXPECTED_VALUE = 0.05
STARTING_BANKROLL = 100.0
UNIT_STAKE = 1.0

CLASSES = [0, 1, 2]
CLASS_NAMES = ["Home Win", "Draw", "Away Win"]

STAT_ALIASES = {
    "xg": {
        "home": ["HomeXG", "home_xg", "HxG", "xG_Home", "xG"],
        "away": ["AwayXG", "away_xg", "AxG", "xG_Away", "xGA"],
    },
    "shots": {
        "home": ["HS", "HomeShots", "home_shots"],
        "away": ["AS", "AwayShots", "away_shots"],
    },
    "shots_on_target": {
        "home": ["HST", "HomeShotsOnTarget", "home_sot"],
        "away": ["AST", "AwayShotsOnTarget", "away_sot"],
    },
    "corners": {
        "home": ["HC", "HomeCorners", "home_corners"],
        "away": ["AC", "AwayCorners", "away_corners"],
    },
}

ODDS_ALIASES = {
    "home": ["B365H", "AvgH", "PSH", "WHH", "HomeOdds"],
    "draw": ["B365D", "AvgD", "PSD", "WHD", "DrawOdds"],
    "away": ["B365A", "AvgA", "PSA", "WHA", "AwayOdds"],
}


def find_column(dataframe, candidates):
    for candidate in candidates:
        if candidate in dataframe.columns:
            return candidate
    return None


# ----------------------------------------------------------------------------
# 1. Load and validate
# ----------------------------------------------------------------------------
print("1. Loading match data...")
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)  # original crashed if missing

data_path = next((p for p in DATA_PATHS if p.exists()), None)
if data_path is None:
    raise FileNotFoundError(f"Dataset not found. Looked in: {DATA_PATHS}")

df = pd.read_csv(data_path, encoding="utf-8-sig")

required = ["Date", "HomeTeam", "AwayTeam", "FTHG", "FTAG", "FTR"]
missing = [c for c in required if c not in df.columns]
if missing:
    raise ValueError(f"Missing required columns: {missing}")

df["Date"] = pd.to_datetime(df["Date"], format="mixed", errors="coerce")
df["HomeTeam"] = df["HomeTeam"].astype(str).str.strip()
df["AwayTeam"] = df["AwayTeam"].astype(str).str.strip()
df["FTHG"] = pd.to_numeric(df["FTHG"], errors="coerce")
df["FTAG"] = pd.to_numeric(df["FTAG"], errors="coerce")
df["FTR"] = df["FTR"].astype(str).str.strip().str.upper()

df = df.dropna(subset=required)
df = df[
    df["FTR"].isin(["H", "D", "A"])
    & (df["FTHG"] >= 0)
    & (df["FTAG"] >= 0)
    & (df["HomeTeam"] != df["AwayTeam"])
].copy()

actual = np.where(
    df["FTHG"] > df["FTAG"], "H", np.where(df["FTHG"] < df["FTAG"], "A", "D")
)
df = df[df["FTR"].to_numpy() == actual].copy()

df = (
    df.sort_values("Date", kind="stable")
    .drop_duplicates(subset=["Date", "HomeTeam", "AwayTeam"], keep="last")
    .reset_index(drop=True)
)
df["Target"] = df["FTR"].map({"H": 0, "D": 1, "A": 2}).astype(int)

if len(df) < 300:
    raise ValueError("Insufficient match data for reliable evaluation.")

# Season id. Prefer the merged file's column. The fallback uses August as the
# season start so the COVID-delayed 2019/20 finish (July 2020) stays in 2019/20
# (the original July-1 rule wrongly treated it as the start of a new season).
fallback_season = np.where(
    df["Date"].dt.month >= 8, df["Date"].dt.year, df["Date"].dt.year - 1
)
if "SeasonStartYear" in df.columns:
    df["SeasonId"] = (
        pd.to_numeric(df["SeasonStartYear"], errors="coerce")
        .fillna(pd.Series(fallback_season, index=df.index))
        .astype(int)
    )
else:
    df["SeasonId"] = fallback_season.astype(int)
SEASON_START = df.groupby("SeasonId")["Date"].min().to_dict()

STAT_COLUMNS = {}
for stat_name, aliases in STAT_ALIASES.items():
    home_col = find_column(df, aliases["home"])
    away_col = find_column(df, aliases["away"])
    if home_col and away_col:
        df[home_col] = pd.to_numeric(df[home_col], errors="coerce")
        df[away_col] = pd.to_numeric(df[away_col], errors="coerce")
        STAT_COLUMNS[stat_name] = {"home": home_col, "away": away_col}

ODDS_COLUMNS = {k: find_column(df, v) for k, v in ODDS_ALIASES.items()}
if not all(ODDS_COLUMNS.values()):
    ODDS_COLUMNS = {}
if ODDS_COLUMNS:
    for col in ODDS_COLUMNS.values():
        df[col] = pd.to_numeric(df[col], errors="coerce")
    odds_matrix = df[list(ODDS_COLUMNS.values())].to_numpy(dtype=float)
    ok = np.all(np.isfinite(odds_matrix) & (odds_matrix > 1.0), axis=1)
    if ok.sum() < len(df) * 0.5:
        print("   Warning: too few complete odds rows; odds disabled.")
        ODDS_COLUMNS = {}

print(f"   Source: {data_path}")
print(f"   Matches: {len(df):,}")
print(f"   Date range: {df['Date'].min().date()} to {df['Date'].max().date()}")
print(f"   Match statistics found: {list(STAT_COLUMNS)}")
print(f"   Bookmaker odds available: {bool(ODDS_COLUMNS)}")


# ----------------------------------------------------------------------------
# 2. Feature engineering (strictly pre-match)
# ----------------------------------------------------------------------------
def summarize(records):
    """Summarise a list of past matches for one team."""
    if not records:
        return {
            "gf": 1.4, "ga": 1.4, "points": 1.3, "gd": 0.0,
            "wins": 0.40, "draws": 0.25, "wpoints": 1.3, "low": 0.5,
            "stats_for": {s: np.nan for s in STAT_COLUMNS},
            "stats_against": {s: np.nan for s in STAT_COLUMNS},
        }
    gf = np.array([r["gf"] for r in records], dtype=float)
    ga = np.array([r["ga"] for r in records], dtype=float)
    pts = np.array([r["points"] for r in records], dtype=float)
    weights = np.arange(1, len(records) + 1, dtype=float)

    stats_for, stats_against = {}, {}
    for s in STAT_COLUMNS:
        vals_for = np.array([r["stats_for"][s] for r in records], dtype=float)
        vals_against = np.array([r["stats_against"][s] for r in records], dtype=float)
        stats_for[s] = float(np.nanmean(vals_for)) if np.isfinite(vals_for).any() else np.nan
        stats_against[s] = float(np.nanmean(vals_against)) if np.isfinite(vals_against).any() else np.nan

    return {
        "gf": gf.mean(),
        "ga": ga.mean(),
        "points": pts.mean(),
        "gd": (gf - ga).mean(),
        "wins": float(np.mean(pts == 3.0)),
        "draws": float(np.mean(pts == 1.0)),
        "wpoints": float(np.average(pts, weights=weights)),
        "low": float(np.mean((gf + ga) <= 2.0)),  # low-scoring games draw more
        "stats_for": stats_for,
        "stats_against": stats_against,
    }


def schedule(records, date):
    if not records:
        return 7.0, 0.0
    rest = min(30.0, float((date - records[-1]["date"]).days))
    busy = 0
    for r in reversed(records[-8:]):
        if (date - r["date"]).days <= 14:
            busy += 1
        else:
            break
    return rest, float(busy)


def market_probabilities(match):
    if not ODDS_COLUMNS:
        return None
    odds = np.array(
        [match[ODDS_COLUMNS[k]] for k in ("home", "draw", "away")], dtype=float
    )
    if not np.all(np.isfinite(odds) & (odds > 1.0)):
        return None
    implied = 1.0 / odds
    return implied / implied.sum()  # remove the bookmaker margin


class Tracker:
    """Holds team history and Elo ratings as the season loop advances."""

    def __init__(self):
        self.history = defaultdict(list)
        self.venue_history = defaultdict(list)
        self.elo = {}
        self.new_team_elo = INITIAL_ELO
        self.season = None

    def rating(self, team):
        if team not in self.elo:
            self.elo[team] = self.new_team_elo
        return self.elo[team]

    def start_season(self, season):
        if self.season is not None and season != self.season:
            for team in self.elo:  # regress to the mean between seasons
                self.elo[team] = INITIAL_ELO + (1 - SEASON_REGRESSION) * (
                    self.elo[team] - INITIAL_ELO
                )
            self.new_team_elo = PROMOTED_ELO  # promoted sides are weaker
        self.season = season

    def features(self, match):
        home, away, date = match["HomeTeam"], match["AwayTeam"], match["Date"]
        hh, ah = self.history[home], self.history[away]

        h5, a5 = summarize(hh[-5:]), summarize(ah[-5:])
        h10, a10 = summarize(hh[-10:]), summarize(ah[-10:])
        hv = summarize(self.venue_history[(home, "H")][-10:])
        av = summarize(self.venue_history[(away, "A")][-10:])

        home_rest, home_busy = schedule(hh, date)
        away_rest, away_busy = schedule(ah, date)

        home_elo, away_elo = self.rating(home), self.rating(away)
        diff = home_elo + HOME_ADVANTAGE - away_elo
        exp_home = 1.0 / (1.0 + 10.0 ** (-diff / 400.0))
        days = float(max(0, (date - SEASON_START[match["SeasonId"]]).days))

        f = {
            "elo_difference": diff,
            "elo_expected_home": exp_home,
            "home_elo": home_elo,
            "away_elo": away_elo,
            "elo_absolute_difference": abs(diff),
            "elo_closeness": float(np.exp(-((diff / 150.0) ** 2))),
            "home_form_points_5": h5["points"],
            "away_form_points_5": a5["points"],
            "home_form_points_10": h10["points"],
            "away_form_points_10": a10["points"],
            "home_weighted_points_5": h5["wpoints"],
            "away_weighted_points_5": a5["wpoints"],
            "form_points_gap_5": abs(h5["points"] - a5["points"]),
            "home_goals_for_5": h5["gf"],
            "home_goals_against_5": h5["ga"],
            "away_goals_for_5": a5["gf"],
            "away_goals_against_5": a5["ga"],
            "home_goal_difference_5": h5["gd"],
            "away_goal_difference_5": a5["gd"],
            "home_win_rate_5": h5["wins"],
            "away_win_rate_5": a5["wins"],
            "home_draw_rate_5": h5["draws"],
            "away_draw_rate_5": a5["draws"],
            "home_draw_rate_10": h10["draws"],
            "away_draw_rate_10": a10["draws"],
            "home_home_goals_for": hv["gf"],
            "home_home_goals_against": hv["ga"],
            "home_home_points": hv["points"],
            "away_away_goals_for": av["gf"],
            "away_away_goals_against": av["ga"],
            "away_away_points": av["points"],
            "home_rest_days": home_rest,
            "away_rest_days": away_rest,
            "home_matches_last_14_days": home_busy,
            "away_matches_last_14_days": away_busy,
            "days_into_season": days,
            # Draw-oriented signals
            "combined_goals_5": h5["gf"] + a5["gf"],
            "combined_goals_10": h10["gf"] + a10["gf"],
            "combined_draw_rate_5": (h5["draws"] + a5["draws"]) / 2.0,
            "combined_draw_rate_10": (h10["draws"] + a10["draws"]) / 2.0,
            "combined_low_scoring_5": (h5["low"] + a5["low"]) / 2.0,
            "expected_total_goals": (
                (h5["gf"] + a5["ga"]) / 2.0 + (a5["gf"] + h5["ga"]) / 2.0
            ),
        }

        for s in STAT_COLUMNS:
            f[f"home_{s}_for_5"] = h5["stats_for"][s]
            f[f"home_{s}_against_5"] = h5["stats_against"][s]
            f[f"away_{s}_for_5"] = a5["stats_for"][s]
            f[f"away_{s}_against_5"] = a5["stats_against"][s]

        if ODDS_COLUMNS and USE_MARKET_FEATURES:
            m = market_probabilities(match)
            if m is None:
                m = np.array([np.nan, np.nan, np.nan])
            f["market_home_probability"] = m[0]
            f["market_draw_probability"] = m[1]
            f["market_away_probability"] = m[2]
            f["market_closeness"] = 1.0 - abs(m[0] - m[2])  # close games draw more
        return f

    def update_elo(self, day_matches):
        changes = defaultdict(float)
        for m in day_matches:
            home, away = m["HomeTeam"], m["AwayTeam"]
            hr, ar = self.rating(home), self.rating(away)
            expected = 1.0 / (1.0 + 10.0 ** (-((hr + HOME_ADVANTAGE) - ar) / 400.0))
            score = {"H": 1.0, "D": 0.5, "A": 0.0}[m["FTR"]]
            margin = abs(float(m["FTHG"]) - float(m["FTAG"]))
            mult = (
                np.log1p(margin) * 2.2 / (abs(hr - ar) * 0.001 + 2.2)
                if margin > 0 else 1.0
            )
            delta = ELO_K * mult * (score - expected)
            changes[home] += delta
            changes[away] -= delta
        for team, delta in changes.items():
            self.elo[team] += delta

    def record(self, m):
        home, away = m["HomeTeam"], m["AwayTeam"]
        hg, ag = float(m["FTHG"]), float(m["FTAG"])
        if hg > ag:
            hp, ap = 3.0, 0.0
        elif hg < ag:
            hp, ap = 0.0, 3.0
        else:
            hp, ap = 1.0, 1.0
        h_stats = {s: float(m[c["home"]]) for s, c in STAT_COLUMNS.items()}
        a_stats = {s: float(m[c["away"]]) for s, c in STAT_COLUMNS.items()}
        home_rec = {"date": m["Date"], "gf": hg, "ga": ag, "points": hp,
                    "stats_for": h_stats, "stats_against": a_stats}
        away_rec = {"date": m["Date"], "gf": ag, "ga": hg, "points": ap,
                    "stats_for": a_stats, "stats_against": h_stats}
        self.history[home].append(home_rec)
        self.history[away].append(away_rec)
        self.venue_history[(home, "H")].append(home_rec)
        self.venue_history[(away, "A")].append(away_rec)


print("2. Engineering chronological features...")
tracker = Tracker()
feature_rows, market_rows = [], []
row_order = []

for date, day in df.groupby("Date", sort=True):
    records = day.to_dict("records")
    tracker.start_season(records[0]["SeasonId"])
    for match in records:  # features use only matches from earlier dates
        feature_rows.append(tracker.features(match))
        market_rows.append(market_probabilities(match))
    tracker.update_elo(records)
    for match in records:
        tracker.record(match)
    row_order.extend(day.index.tolist())

# groupby(sort=True) walks dates in order; align df to that order
df = df.loc[row_order].reset_index(drop=True)
features_df = pd.DataFrame(feature_rows).astype("float32")
market_all = np.array(
    [m if m is not None else [np.nan] * 3 for m in market_rows], dtype=float
)
y_all = df["Target"].to_numpy()
home_goals_all = df["FTHG"].to_numpy(dtype=float)
away_goals_all = df["FTAG"].to_numpy(dtype=float)

print(f"   Feature matrix: {features_df.shape}")
print(
    f"   Class balance: Home {np.mean(y_all == 0):.1%} | "
    f"Draw {np.mean(y_all == 1):.1%} | Away {np.mean(y_all == 2):.1%}"
)

# ----------------------------------------------------------------------------
# 3. Chronological train / validation / test split
# ----------------------------------------------------------------------------
print("3. Creating chronological train / validation / test sets...")
unique_dates = np.sort(df["Date"].unique())
n_dates = len(unique_dates)
val_start = pd.Timestamp(unique_dates[int(n_dates * (1 - VAL_SIZE - TEST_SIZE))])
test_start = pd.Timestamp(unique_dates[int(n_dates * (1 - TEST_SIZE))])

train_mask = (df["Date"] < val_start).to_numpy()
val_mask = ((df["Date"] >= val_start) & (df["Date"] < test_start)).to_numpy()
test_mask = (df["Date"] >= test_start).to_numpy()
trainval_mask = train_mask | val_mask

# Drop features that are mostly empty in the data used for fitting
# (e.g. xG exists only for 2026/27). Done before any model sees the data.
missing_rate = features_df[trainval_mask].isna().mean()
dropped = missing_rate[missing_rate > MAX_MISSING_RATE].index.tolist()
if dropped:
    print(f"   Dropping mostly-empty features: {dropped}")
FEATURE_NAMES = [c for c in features_df.columns if c not in dropped]
X_all = features_df[FEATURE_NAMES].to_numpy(dtype=np.float32)
print(f"   Features used: {len(FEATURE_NAMES)}")

X_train, y_train = X_all[train_mask], y_all[train_mask]
X_val, y_val = X_all[val_mask], y_all[val_mask]
X_test, y_test = X_all[test_mask], y_all[test_mask]
X_tv, y_tv = X_all[trainval_mask], y_all[trainval_mask]

for name, arr in (("train", y_train), ("validation", y_val), ("test", y_test)):
    if len(np.unique(arr)) != 3:
        raise ValueError(f"The {name} set does not contain all three outcomes.")

test_df = df.loc[test_mask].reset_index(drop=True)
market_test = market_all[test_mask]
market_val = market_all[val_mask]

print(f"   Train: {len(X_train)}  ({df.loc[train_mask, 'Date'].min().date()} - {df.loc[train_mask, 'Date'].max().date()})")
print(f"   Validation: {len(X_val)}  ({val_start.date()} - {df.loc[val_mask, 'Date'].max().date()})")
print(f"   Test: {len(X_test)}  ({test_start.date()} - {df['Date'].max().date()})")


# ----------------------------------------------------------------------------
# 4. Models
# ----------------------------------------------------------------------------
def make_rf():
    # No class_weight: balanced weights distort probabilities. Draw handling is
    # done by the decision rule below, which keeps the probabilities honest.
    return Pipeline([
        ("imputer", SimpleImputer(strategy="median")),
        ("classifier", RandomForestClassifier(
            n_estimators=500,
            max_depth=8,
            min_samples_split=20,
            min_samples_leaf=10,
            max_features=0.4,
            random_state=RANDOM_STATE,
            n_jobs=-1,
        )),
    ])


def make_calibrated_rf():
    kwargs = dict(method="sigmoid", cv=TimeSeriesSplit(n_splits=4), ensemble=True)
    try:
        return CalibratedClassifierCV(estimator=make_rf(), **kwargs)
    except TypeError:  # scikit-learn < 1.2
        return CalibratedClassifierCV(base_estimator=make_rf(), **kwargs)


def fit_models(X, y, home_goals, away_goals):
    models = {"rf": make_rf().fit(X, y)}
    models["rf_cal"] = make_calibrated_rf().fit(X, y) if len(X) >= 500 else None
    prep = Pipeline([("imputer", SimpleImputer(strategy="median")),
                     ("scaler", StandardScaler())]).fit(X)
    Xs = prep.transform(X)
    models["prep"] = prep
    models["home_goals"] = PoissonRegressor(alpha=0.5, max_iter=1000).fit(Xs, home_goals)
    models["away_goals"] = PoissonRegressor(alpha=0.5, max_iter=1000).fit(Xs, away_goals)
    return models


def poisson_probabilities(lam_home, lam_away, draw_inflation=0.0, max_goals=10):
    """Scoreline-matrix probabilities. `draw_inflation` boosts the diagonal:
    independent Poisson goals systematically under-predict draws."""
    goals = np.arange(max_goals + 1)
    out = np.empty((len(lam_home), 3))
    for i, (lh, la) in enumerate(zip(lam_home, lam_away)):
        matrix = np.outer(poisson.pmf(goals, lh), poisson.pmf(goals, la))
        draw = np.trace(matrix) * (1.0 + draw_inflation)
        home = np.tril(matrix, k=-1).sum()
        away = np.triu(matrix, k=1).sum()
        probs = np.maximum([home, draw, away], 1e-12)
        out[i] = probs / probs.sum()
    return out


def raw_components(models, X):
    rf_raw = models["rf"].predict_proba(X)[:, CLASSES]
    rf_cal = models["rf_cal"].predict_proba(X)[:, CLASSES] if models["rf_cal"] is not None else rf_raw
    Xs = models["prep"].transform(X)
    lam_h = np.clip(models["home_goals"].predict(Xs), 0.05, 6.0)
    lam_a = np.clip(models["away_goals"].predict(Xs), 0.05, 6.0)
    return rf_raw, rf_cal, lam_h, lam_a


def normalise(p):
    p = np.clip(np.asarray(p, dtype=float), 1e-12, 1.0)
    return p / p.sum(axis=1, keepdims=True)


def evaluate_probabilities(name, probs, actual):
    probs = normalise(probs)
    pred = probs.argmax(axis=1)
    return {
        "Model": name,
        "Accuracy": accuracy_score(actual, pred),
        "LogLoss": log_loss(actual, probs, labels=CLASSES),
        "Brier": float(np.mean(np.sum((probs - np.eye(3)[actual]) ** 2, axis=1))),
    }


# ----------------------------------------------------------------------------
# 5. Tune on the validation window (never on test)
# ----------------------------------------------------------------------------
print("4. Fitting on train, tuning on validation...")
train_models = fit_models(
    X_train, y_train, home_goals_all[train_mask], away_goals_all[train_mask]
)
v_rf_raw, v_rf_cal, v_lam_h, v_lam_a = raw_components(train_models, X_val)

# 5a. Poisson draw inflation (lowest validation log loss)
inflation_grid = np.round(np.arange(0.0, 0.81, 0.05), 2)
inflation_losses = [
    log_loss(y_val, poisson_probabilities(v_lam_h, v_lam_a, d), labels=CLASSES)
    for d in inflation_grid
]
DRAW_INFLATION = float(inflation_grid[int(np.argmin(inflation_losses))])
v_poisson = poisson_probabilities(v_lam_h, v_lam_a, DRAW_INFLATION)
print(f"   Poisson draw inflation: {DRAW_INFLATION:.2f}")

# 5b. Ensemble weight between calibrated RF and Poisson
weight_grid = np.round(np.arange(0.0, 1.01, 0.1), 2)
weight_losses = [
    log_loss(y_val, normalise(w * v_rf_cal + (1 - w) * v_poisson), labels=CLASSES)
    for w in weight_grid
]
RF_WEIGHT = float(weight_grid[int(np.argmin(weight_losses))])
v_ensemble = normalise(RF_WEIGHT * v_rf_cal + (1 - RF_WEIGHT) * v_poisson)
print(f"   Ensemble weight (RF / Poisson): {RF_WEIGHT:.1f} / {1 - RF_WEIGHT:.1f}")


# 5c. Draw-aware decision rule
def decision_score(actual, pred):
    if DECISION_OBJECTIVE == "balanced_accuracy":
        return balanced_accuracy_score(actual, pred)
    return f1_score(actual, pred, labels=CLASSES, average="macro", zero_division=0)


def tune_decision_weights(probs, actual):
    """Pick multipliers (1, w_draw, w_away) applied to the probabilities before
    argmax. A 3x3 smoothing filter avoids chasing one lucky grid cell."""
    draw_grid = np.round(np.arange(0.90, 2.51, 0.05), 2)
    away_grid = np.round(np.arange(0.80, 1.41, 0.05), 2)
    scores = np.zeros((len(draw_grid), len(away_grid)))
    for i, wd in enumerate(draw_grid):
        for j, wa in enumerate(away_grid):
            pred = np.argmax(probs * np.array([1.0, wd, wa]), axis=1)
            scores[i, j] = decision_score(actual, pred)
    smooth = uniform_filter(scores, size=3, mode="nearest")
    i, j = np.unravel_index(np.argmax(smooth), smooth.shape)
    return np.array([1.0, draw_grid[i], away_grid[j]]), scores[i, j]


DECISION_WEIGHTS, val_score = tune_decision_weights(v_ensemble, y_val)
v_pred_plain = v_ensemble.argmax(axis=1)
v_pred_tuned = np.argmax(v_ensemble * DECISION_WEIGHTS, axis=1)
print(
    f"   Decision weights (Home, Draw, Away): {DECISION_WEIGHTS.round(2).tolist()}"
)
print(
    f"   Validation {DECISION_OBJECTIVE}: argmax "
    f"{decision_score(y_val, v_pred_plain):.3f} -> tuned {val_score:.3f}"
)
print(
    f"   Validation draws predicted: argmax {np.mean(v_pred_plain == 1):.1%} "
    f"-> tuned {np.mean(v_pred_tuned == 1):.1%} (actual {np.mean(y_val == 1):.1%})"
)

# ----------------------------------------------------------------------------
# 6. Refit on train+validation and score once on test
# ----------------------------------------------------------------------------
print("\n5. Refitting on train + validation and evaluating on test...")
final_models = fit_models(
    X_tv, y_tv, home_goals_all[trainval_mask], away_goals_all[trainval_mask]
)
rf_raw, rf_cal, lam_h, lam_a = raw_components(final_models, X_test)
poisson_probs = poisson_probabilities(lam_h, lam_a, DRAW_INFLATION)
ensemble_probs = normalise(RF_WEIGHT * rf_cal + (1 - RF_WEIGHT) * poisson_probs)

# Elo baseline (transparent reference)
draw_rate = float(np.clip(np.mean(y_tv == 1), 0.15, 0.35))
elo_col = FEATURE_NAMES.index("elo_expected_home")
exp_home = X_test[:, elo_col].astype(float)
elo_probs = np.column_stack([
    (1 - draw_rate) * exp_home,
    np.full(len(exp_home), draw_rate),
    (1 - draw_rate) * (1 - exp_home),
])

evaluation = [
    evaluate_probabilities("Random Forest", rf_raw, y_test),
    evaluate_probabilities("Calibrated Random Forest", rf_cal, y_test),
    evaluate_probabilities("Poisson (draw-inflated)", poisson_probs, y_test),
    evaluate_probabilities("Elo baseline", elo_probs, y_test),
    evaluate_probabilities("Ensemble (RF + Poisson)", ensemble_probs, y_test),
]
if ODDS_COLUMNS:
    market_filled = np.where(np.isfinite(market_test), market_test, 1 / 3)
    evaluation.append(evaluate_probabilities("Bookmaker baseline", market_filled, y_test))

print("\n   Probability quality on test (lower LogLoss / Brier is better):")
print(
    pd.DataFrame(evaluation)
    .sort_values("LogLoss")
    .to_string(index=False, float_format=lambda v: f"{v:.4f}")
)

pred_plain = ensemble_probs.argmax(axis=1)
pred_tuned = np.argmax(ensemble_probs * DECISION_WEIGHTS, axis=1)


def decision_summary(name, pred):
    draw_precision = (
        np.mean(y_test[pred == 1] == 1) if np.any(pred == 1) else 0.0
    )
    draw_recall = np.mean(pred[y_test == 1] == 1)
    return {
        "Rule": name,
        "Accuracy": accuracy_score(y_test, pred),
        "BalancedAcc": balanced_accuracy_score(y_test, pred),
        "MacroF1": f1_score(y_test, pred, labels=CLASSES, average="macro", zero_division=0),
        "DrawsPredicted": int(np.sum(pred == 1)),
        "DrawsActual": int(np.sum(y_test == 1)),
        "DrawPrecision": draw_precision,
        "DrawRecall": draw_recall,
    }


print("\n   Hard predictions on test: plain argmax vs draw-aware rule")
print(
    pd.DataFrame([
        decision_summary("Plain argmax", pred_plain),
        decision_summary("Draw-aware", pred_tuned),
    ]).to_string(index=False, float_format=lambda v: f"{v:.3f}")
)

print("\n   Draw-aware classification report:")
print(classification_report(
    y_test, pred_tuned, labels=CLASSES, target_names=CLASS_NAMES, zero_division=0
))
print("   Draw-aware confusion matrix (rows = actual, cols = predicted):")
print(pd.DataFrame(
    confusion_matrix(y_test, pred_tuned, labels=CLASSES),
    index=[f"actual {n}" for n in CLASS_NAMES],
    columns=[f"pred {n}" for n in CLASS_NAMES],
).to_string())

# ----------------------------------------------------------------------------
# 7. Betting backtest (uses probabilities, not the draw-aware labels)
# ----------------------------------------------------------------------------
print("\n6. Running historical betting backtest...")
if ODDS_COLUMNS:
    bankroll = peak = STARTING_BANKROLL
    max_drawdown = total_profit = total_staked = 0.0
    bets = wins = 0
    bet_records = []
    for index, match in test_df.iterrows():
        odds = np.array([match[ODDS_COLUMNS[k]] for k in ("home", "draw", "away")], dtype=float)
        if not np.all(np.isfinite(odds) & (odds > 1.0)):
            continue
        probs = ensemble_probs[index]
        expected_returns = probs * odds - 1.0
        pick = int(np.argmax(expected_returns))
        ev = float(expected_returns[pick])
        if ev < MIN_EXPECTED_VALUE:
            continue
        outcome = int(match["Target"])
        profit = UNIT_STAKE * (odds[pick] - 1.0) if pick == outcome else -UNIT_STAKE
        bankroll += profit
        total_profit += profit
        total_staked += UNIT_STAKE
        bets += 1
        wins += int(pick == outcome)
        peak = max(peak, bankroll)
        max_drawdown = max(max_drawdown, peak - bankroll)
        bet_records.append({
            "Date": match["Date"], "HomeTeam": match["HomeTeam"],
            "AwayTeam": match["AwayTeam"], "Selection": CLASS_NAMES[pick],
            "Actual": CLASS_NAMES[outcome], "Probability": probs[pick],
            "Odds": odds[pick], "ExpectedValue": ev, "Profit": profit,
            "Bankroll": bankroll,
        })
    if bets:
        print(f"   Bets placed: {bets}  |  Win rate: {wins / bets:.2%}")
        print(f"   Staked: {total_staked:.2f}  |  Net profit: {total_profit:.2f} units")
        print(f"   ROI: {total_profit / total_staked:.2%}  |  Final bankroll: {bankroll:.2f}")
        print(f"   Maximum drawdown: {max_drawdown:.2f} units")
        pd.DataFrame(bet_records).to_csv(BACKTEST_PATH, index=False)
    else:
        print("   No qualifying bets found.")
else:
    print("   Skipped: no complete decimal odds columns available.")

# ----------------------------------------------------------------------------
# 8. Reports
# ----------------------------------------------------------------------------
print("\n7. Saving reports...")
out = test_df[["Date", "HomeTeam", "AwayTeam", "FTHG", "FTAG", "FTR"]].copy()
out["P_HomeWin"] = ensemble_probs[:, 0]
out["P_Draw"] = ensemble_probs[:, 1]
out["P_AwayWin"] = ensemble_probs[:, 2]
out["Predicted_Argmax"] = [CLASS_NAMES[i] for i in pred_plain]
out["Predicted"] = [CLASS_NAMES[i] for i in pred_tuned]  # draw-aware
out["ExpectedHomeGoals"] = lam_h
out["ExpectedAwayGoals"] = lam_a
out.to_csv(RESULTS_PATH, index=False)

rf = final_models["rf"].named_steps["classifier"]
importance = pd.Series(
    rf.feature_importances_, index=FEATURE_NAMES, name="importance"
).sort_values(ascending=False)
importance.to_csv(IMPORTANCE_PATH, header=True)
print("\n   Top 15 features:")
print(importance.head(15).to_string())

config = {
    "feature_names": FEATURE_NAMES,
    "class_names": CLASS_NAMES,
    "decision_weights_home_draw_away": DECISION_WEIGHTS.tolist(),
    "decision_objective": DECISION_OBJECTIVE,
    "ensemble_rf_weight": RF_WEIGHT,
    "poisson_draw_inflation": DRAW_INFLATION,
    "usage": "Multiply the model's [home, draw, away] probabilities by "
             "decision_weights_home_draw_away, then take the argmax.",
}
CONFIG_PATH.write_text(json.dumps(config, indent=2))

# ----------------------------------------------------------------------------
# 9. ONNX export of the Random Forest pipeline
# ----------------------------------------------------------------------------
print("\n8. Exporting ONNX model...")
try:
    from skl2onnx import convert_sklearn
    from skl2onnx.common.data_types import FloatTensorType

    onnx_model = convert_sklearn(
        final_models["rf"],
        initial_types=[("float_input", FloatTensorType([None, len(FEATURE_NAMES)]))],
        target_opset=17,
        options={id(final_models["rf"].named_steps["classifier"]): {"zipmap": False}},
    )
    MODEL_PATH.write_bytes(onnx_model.SerializeToString())
    print(f"   ONNX model: {MODEL_PATH}")

    try:  # verify the export reproduces scikit-learn
        import onnxruntime as ort

        session = ort.InferenceSession(str(MODEL_PATH), providers=["CPUExecutionProvider"])
        onnx_probs = session.run(None, {"float_input": X_test.astype(np.float32)})[1]
        gap = float(np.max(np.abs(onnx_probs - rf_raw)))
        print(f"   ONNX vs scikit-learn max probability gap: {gap:.2e}")
    except ImportError:
        print("   (install onnxruntime to verify the exported model)")
except ImportError:
    print("   Skipped: install skl2onnx to export (pip install skl2onnx).")

print(f"\n   Test predictions: {RESULTS_PATH}")
print(f"   Feature importance: {IMPORTANCE_PATH}")
print(f"   Decision config: {CONFIG_PATH}")
print("\nTraining completed.")