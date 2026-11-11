#include "SearchPage.h"
#include "Theme.h"
#include <QSplitter>

SearchPage::SearchPage(AppController* c, QWidget* parent) : QWidget(parent), m_c(c) {
    setObjectName("Page");
    auto* v = new QVBoxLayout(this); v->setContentsMargins(28, 24, 28, 24); v->setSpacing(14);
    v->addWidget(makeLabel("Knowledge search", "h1"));

    auto* bar = new Card;
    auto* r = new QHBoxLayout; r->setSpacing(10);
    m_query = new QLineEdit; m_query->setPlaceholderText("Ask e.g.  Where is ONNX initialized?   or   how are odds converted to probabilities"); m_query->setClearButtonEnabled(true);
    auto* go = makeButton("Search", true); auto* rebuild = makeButton("Rebuild index");
    r->addWidget(m_query, 1); r->addWidget(go); r->addWidget(rebuild);
    bar->body()->addLayout(r);
    auto* chips = new QHBoxLayout; chips->setSpacing(6);
    chips->addWidget(makeLabel("Try:", "dim"));
    for (const char* q : {"Where is ONNX initialized?", "feature engineering elo", "reasoning rules thresholds", "API rate limit handling", "sqlite prepared statement"}) {
        auto* b = makeButton(q); b->setProperty("flat", true);
        connect(b, &QPushButton::clicked, this, [this, q]() { m_query->setText(q); search(); });
        chips->addWidget(b);
    }
    chips->addStretch();
    bar->body()->addLayout(chips);
    m_info = makeLabel("", "dim"); bar->body()->addWidget(m_info);
    v->addWidget(bar);

    auto* split = new QSplitter(Qt::Horizontal); split->setChildrenCollapsible(false);
    m_list = new QListWidget; m_list->setWordWrap(true);
    split->addWidget(m_list);
    auto* pv = new Card;
    m_path = makeLabel("No result selected", "mono"); pv->body()->addWidget(m_path);
    m_preview = new QPlainTextEdit; m_preview->setReadOnly(true); m_preview->setFont(Theme::monoFont(9)); m_preview->setLineWrapMode(QPlainTextEdit::NoWrap);
    pv->body()->addWidget(m_preview, 1);
    split->addWidget(pv);
    split->setStretchFactor(0, 2); split->setStretchFactor(1, 3);
    v->addWidget(split, 1);

    connect(m_query, &QLineEdit::returnPressed, this, [this]() { search(); });
    connect(go, &QPushButton::clicked, this, [this]() { search(); });
    connect(rebuild, &QPushButton::clicked, this, [this]() { m_c->rebuildIndex(); updateInfo(); });
    connect(m_c, &AppController::indexChanged, this, [this]() { updateInfo(); });
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= static_cast<int>(m_hits.size())) return;
        const auto& h = m_hits[row];
        m_path->setText(QString("%1  (lines %2-%3)").arg(QString::fromStdString(h.file)).arg(h.startLine).arg(h.endLine));
        QString out; int n = h.startLine;
        for (const auto& line : QString::fromStdString(h.text).split('\n')) { if (n > h.endLine) break; out += QString("%1  %2\n").arg(n++, 4).arg(line); }
        m_preview->setPlainText(out);
    });
    updateInfo();
}

void SearchPage::updateInfo() {
    auto idx = m_c->index();
    if (m_c->indexBuilding()) m_info->setText("Indexing project files...");
    else m_info->setText(QString("Indexed %1 sections from %2 files (%3 terms) under %4. Build folders, .git, secrets and binaries are excluded.")
                             .arg(idx->chunkCount()).arg(idx->fileCount()).arg(idx->termCount()).arg(m_c->indexRoot()));
}

void SearchPage::search() {
    const QString q = m_query->text().trimmed();
    m_list->clear(); m_hits.clear(); m_preview->clear(); m_path->setText("No result selected");
    if (q.isEmpty()) return;
    auto idx = m_c->index();
    if (idx->chunkCount() == 0) { m_info->setText(m_c->indexBuilding() ? "The index is still being built - try again in a moment." : "The index is empty. Check that the project folder exists."); return; }
    idx->refreshIfChanged(idx->root());       // picks up edited files before searching
    m_hits = idx->search(q.toStdString(), 12);
    if (m_hits.empty()) { m_list->addItem("No matching sections. Try different or fewer words."); return; }
    for (const auto& h : m_hits) {
        auto* item = new QListWidgetItem(QString("%1:%2-%3     score %4\n%5").arg(QString::fromStdString(h.file)).arg(h.startLine).arg(h.endLine)
                                             .arg(h.score, 0, 'f', 3).arg(QString::fromStdString(h.snippet).trimmed()));
        item->setSizeHint(QSize(0, 92));
        m_list->addItem(item);
    }
    m_list->setCurrentRow(0);
}
