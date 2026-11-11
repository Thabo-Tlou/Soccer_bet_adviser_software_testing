#pragma once
// Every betting market for one match, priced from the model: result, double chance, draw no bet, over/under, both teams to
// score, team goals, Asian handicap, exact goals, correct score, clean sheets and more. Type the bookmaker's odds to get a
// verdict (value / marginal / no value) and a suggested stake. Used by the customer match page and the admin analysis page.
#include <QFrame>
#include <QHash>
#include <QLabel>
#include <QTableWidget>
#include <QWidget>
#include <vector>
#include "AppController.h"
#include "core/Markets.h"

class QComboBox;
class QDoubleSpinBox;
class QPushButton;

// Bankroll + risk profile controls shared by the markets panel and the bet slip.
class StakeBar : public QFrame {
    Q_OBJECT
public:
    explicit StakeBar(AppController* c, QWidget* parent = nullptr);
private:
    void sync();
    AppController* m_c;
    QDoubleSpinBox* m_bankroll;
    QComboBox* m_risk;
    QLabel* m_limits;
};

class MarketsPanel : public QWidget {
    Q_OBJECT
public:
    enum Mode { Customer, Admin };
    MarketsPanel(AppController* c, Mode mode, QWidget* parent = nullptr);

    void setBook(const sba::MarketBook& book, const QString& matchKey, const QString& matchLabel);
    void clearBook(const QString& message);
    void presetOdds(const std::vector<std::pair<std::string, double>>& odds);   // e.g. odds already entered elsewhere
    void devFill(const std::pair<const char*, double>* picks, size_t n);   // dev aid: odds = fair odds x factor

signals:
    void addToSlip(const sba::SlipLeg& leg);

private:
    struct Row { bool header = false; QString title, blurb; const sba::Selection* sel = nullptr; };
    void rebuild();
    void refreshRow(int row);
    void refreshSummary();
    void refreshAdvice();
    void refreshTrustNote();
    QString money(double v) const;
    double shrinkFor(const sba::Selection& s) const;
    double typedOdds(const std::string& key) const;
    double oppositeOdds(const std::string& key) const;
    sba::BetAdvice adviceFor(const sba::Selection& s) const;
    const sba::Selection* selected() const;

    AppController* m_c;
    Mode m_mode;
    sba::MarketBook m_book;
    QString m_matchKey, m_matchLabel;
    QHash<QString, QHash<QString, double>> m_odds;     // typed odds per match and selection
    std::vector<Row> m_rows;
    int m_filter = 0;

    QLabel *m_summary, *m_trust, *m_empty, *m_adviceTitle, *m_adviceHead, *m_adviceBody;
    QWidget *m_content, *m_adviceBox;
    QTableWidget* m_table;
    QPushButton* m_slipBtn;
    QList<QPushButton*> m_chips;
    bool m_building = false;
};
