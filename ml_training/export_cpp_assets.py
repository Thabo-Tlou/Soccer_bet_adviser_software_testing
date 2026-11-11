"""
Export everything the C++ application needs besides the ONNX Random Forest.

  models/poisson_model.json      Poisson goal models (imputer + scaler + coefficients)
  fixtures/parity_reference.csv  Python features + RF/Poisson probabilities for the
                                 test window, used by the C++ parity test

It re-uses the feature code in train_model.py (everything before the tuning
step) so that Python and C++ are compared against the *same* definitions.
It does NOT retrain or overwrite the ONNX model.

Run from the ml_training folder:   python export_cpp_assets.py
"""
import json
from pathlib import Path

import numpy as np

src = Path("train_model.py").read_text(encoding="utf-8")
marker = 'print("4. Fitting on train, tuning on validation...")'
namespace = {"__name__": "train_model_features"}
exec(compile(src[: src.index(marker)], "train_model.py", "exec"), namespace)

cfg = json.loads(Path("models/epl_decision_config.json").read_text())
assert cfg["feature_names"] == namespace["FEATURE_NAMES"], "feature list changed - retrain first"
inflation = float(cfg["poisson_draw_inflation"])

X_all = namespace["X_all"]; X_tv = namespace["X_tv"]; X_test = namespace["X_test"]
tv = namespace["trainval_mask"]; test = namespace["test_mask"]
df = namespace["df"]; names = namespace["FEATURE_NAMES"]
hg = namespace["home_goals_all"]; ag = namespace["away_goals_all"]

models = namespace["fit_models"](X_tv, namespace["y_tv"], hg[tv], ag[tv])
prep, hm, am = models["prep"], models["home_goals"], models["away_goals"]
imputer, scaler = prep.named_steps["imputer"], prep.named_steps["scaler"]

Path("models/poisson_model.json").write_text(json.dumps({
    "feature_names": names,
    "imputer_median": [float(v) for v in imputer.statistics_],
    "scaler_mean": [float(v) for v in scaler.mean_],
    "scaler_scale": [float(v) for v in scaler.scale_],
    "home_coef": [float(v) for v in hm.coef_], "home_intercept": float(hm.intercept_),
    "away_coef": [float(v) for v in am.coef_], "away_intercept": float(am.intercept_),
    "draw_inflation": inflation, "max_goals": 10, "lambda_clip": [0.05, 6.0],
}, indent=1))

import onnxruntime as ort
sess = ort.InferenceSession("models/random_forest_epl.onnx", providers=["CPUExecutionProvider"])
rf = sess.run(None, {"float_input": X_all.astype(np.float32)})[1]
Xs = prep.transform(X_all)
lam_h = np.clip(hm.predict(Xs), 0.05, 6.0); lam_a = np.clip(am.predict(Xs), 0.05, 6.0)
pois = namespace["poisson_probabilities"](lam_h, lam_a, inflation)

# Reference rows: the last 600 matches (covers the whole test window).
idx = np.arange(len(df))[-600:]
Path("fixtures").mkdir(exist_ok=True)
with open("fixtures/parity_reference.csv", "w", encoding="utf-8") as f:
    f.write("Date,HomeTeam,AwayTeam,FTR," + ",".join(names)
            + ",rf_h,rf_d,rf_a,po_h,po_d,po_a,lam_h,lam_a\n")
    for i in idx:
        row = df.iloc[i]
        vals = ["" if np.isnan(v) else repr(float(v)) for v in X_all[i]]
        f.write(f"{row['Date'].date()},{row['HomeTeam']},{row['AwayTeam']},{row['FTR']},"
                + ",".join(vals) + ","
                + ",".join(repr(float(v)) for v in rf[i]) + ","
                + ",".join(repr(float(v)) for v in pois[i]) + ","
                + f"{lam_h[i]!r},{lam_a[i]!r}\n")

# Sanity: does our refit Poisson reproduce the shipped test predictions?
import pandas as pd
shipped = pd.read_csv("models/epl_test_predictions.csv")
mine = pois[test]
gap = np.abs(shipped[["P_HomeWin", "P_Draw", "P_AwayWin"]].to_numpy() - mine).max()
print(f"Poisson refit vs shipped epl_test_predictions.csv: max gap {gap:.2e}")

# RF quality on the test window (raw, un-ensembled) for the record.
from sklearn.metrics import log_loss
y = namespace["y_all"]
print("RF (ONNX)   test log-loss:", round(log_loss(y[test], rf[test], labels=[0, 1, 2]), 4))
print("Poisson     test log-loss:", round(log_loss(y[test], pois[test], labels=[0, 1, 2]), 4))
mk = namespace["market_all"][test]
mk = np.where(np.isfinite(mk), mk, 1 / 3)
print("Bookmaker   test log-loss:", round(log_loss(y[test], mk, labels=[0, 1, 2]), 4))
print("Rows exported:", len(idx))
