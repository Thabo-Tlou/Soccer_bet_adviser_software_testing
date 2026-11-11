#include "Theme.h"

namespace Theme {

QString hex(const QColor& c) { return c.name(QColor::HexRgb); }
QColor withAlpha(const QColor& c, int a) { QColor x = c; x.setAlpha(a); return x; }

QFont uiFont(int pt, QFont::Weight w) {
    QFont f("Segoe UI");
    f.setStyleHint(QFont::SansSerif);
    f.setPointSize(pt);
    f.setWeight(w);
    return f;
}

QFont monoFont(int pt) {
    QFont f("Cascadia Mono");
    f.setStyleHint(QFont::Monospace);
    f.setFamilies({"Cascadia Mono", "Consolas", "DejaVu Sans Mono", "monospace"});
    f.setPointSize(pt);
    return f;
}

QString stylesheet() {
    return DS::applyTokens(R"(
* { font-family: "Inter", "Segoe UI", "Noto Sans", sans-serif; font-size: 10pt; outline: 0; }
QMainWindow, QDialog { background: @bg; }
QWidget { color: @text; }
QWidget#Page, QStackedWidget { background: @bg; }
QWidget#TopBar { background: @surface; border-bottom: 1px solid @border; }
QWidget#Sidebar { background: @surface; border-right: 1px solid @border; }
QWidget#StatusBar { background: @surface; border-top: 1px solid @border; }
QWidget#StatusBar QLabel { color: @textFaint; font-size: 8.5pt; }
QToolTip { background: @slate900; color: #ffffff; border: 0; padding: 5px 8px; border-radius: 6px; }

QFrame#Card { background: @surface; border: 1px solid @border; border-radius: 14px; }
QFrame#Card QLabel { background: transparent; }
QFrame#Banner { border-radius: 10px; border: 1px solid @border; }
QFrame#Divider { background: @border; max-height: 1px; min-height: 1px; border: 0; }

QLabel[role="h1"] { font-size: 20pt; font-weight: 700; color: @text; }
QLabel[role="h2"] { font-size: 11.5pt; font-weight: 600; color: @text; }
QLabel[role="muted"] { color: @textMuted; }
QLabel[role="dim"] { color: @textFaint; font-size: 9pt; }
QLabel[role="kpi"] { font-size: 21pt; font-weight: 700; color: @text; }
QLabel[role="mono"] { font-family: "Cascadia Mono", "Consolas", "DejaVu Sans Mono", monospace; color: @textMuted; }
QLabel[role="caption"] { color: @textFaint; font-size: 8pt; font-weight: 700; letter-spacing: 1px; }

QPushButton { background: @surface; border: 1px solid @borderStrong; border-radius: 10px; padding: 8px 16px; color: @slate700; font-weight: 600; }
QPushButton:hover { background: @slate50; border-color: @slate400; color: @text; }
QPushButton:pressed { background: @slate100; }
QPushButton:focus { border: 1px solid @primary; }
QPushButton:disabled { color: @slate400; background: @slate50; border-color: @border; }
QPushButton[primary="true"] { background: @primary; border: 1px solid @primary; color: #ffffff; }
QPushButton[primary="true"]:hover { background: @primaryHover; border-color: @primaryHover; color: #ffffff; }
QPushButton[primary="true"]:pressed { background: @primaryPressed; border-color: @primaryPressed; }
QPushButton[primary="true"]:disabled { background: @leaf200; border-color: @leaf200; color: #ffffff; }
QPushButton[flat="true"] { background: transparent; border: 1px solid transparent; color: @textMuted; }
QPushButton[flat="true"]:hover { color: @text; background: @slate100; }

QLineEdit, QComboBox, QDoubleSpinBox, QSpinBox, QDateEdit, QPlainTextEdit, QTextBrowser, QTextEdit {
    background: @surface; border: 1px solid @borderStrong; border-radius: 10px; padding: 7px 11px; color: @text;
    selection-background-color: @leaf100; selection-color: @leaf800; }
QLineEdit:hover, QComboBox:hover, QDoubleSpinBox:hover, QSpinBox:hover, QDateEdit:hover { border-color: @slate400; }
QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus, QSpinBox:focus, QDateEdit:focus, QPlainTextEdit:focus { border: 1px solid @primary; }
QLineEdit:disabled, QComboBox:disabled, QDoubleSpinBox:disabled { color: @slate400; background: @slate50; }
QTextBrowser, QPlainTextEdit { border: 1px solid @border; }
QComboBox::drop-down { border: 0; width: 26px; }
QComboBox::down-arrow { image: url(:/icons/chevron-down-dark.svg); width: 12px; height: 12px; }
QComboBox QAbstractItemView { background: @surface; border: 1px solid @borderStrong; border-radius: 8px; selection-background-color: @leaf50; selection-color: @leaf800; outline: 0; padding: 4px; }
QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button, QDateEdit::up-button, QDateEdit::down-button { width: 0; border: 0; }
QDateEdit::drop-down { border: 0; width: 24px; }
QDateEdit::down-arrow { image: url(:/icons/chevron-down-dark.svg); width: 12px; height: 12px; }
QCalendarWidget QWidget { background: @surface; color: @text; }
QCalendarWidget QAbstractItemView:enabled { selection-background-color: @primary; selection-color: #ffffff; color: @text; }

QCheckBox { spacing: 9px; color: @text; }
QCheckBox::indicator { width: 17px; height: 17px; border: 1px solid @slate400; border-radius: 5px; background: @surface; }
QCheckBox::indicator:hover { border-color: @primary; }
QCheckBox::indicator:checked { background: @primary; border-color: @primary; image: url(:/icons/check.svg); }

QTableWidget, QTableView, QListWidget, QTreeWidget { background: @surface; alternate-background-color: @slate50; border: 1px solid @border; border-radius: 12px;
    gridline-color: transparent; selection-background-color: @leaf50; selection-color: @leaf800; }
QTableWidget::item, QListWidget::item { padding: 6px 10px; border: 0; border-bottom: 1px solid @slate100; }
QTableWidget::item:hover, QListWidget::item:hover { background: @slate50; }
QTableWidget::item:selected, QListWidget::item:selected { background: @leaf50; color: @leaf800; }
QHeaderView { background: transparent; }
QHeaderView::section { background: @slate50; color: @textFaint; border: 0; border-bottom: 1px solid @border; padding: 9px 10px; font-size: 8.5pt; font-weight: 700; }
QTableCornerButton::section { background: @slate50; border: 0; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: @slate300; border-radius: 4px; min-height: 28px; }
QScrollBar::handle:vertical:hover { background: @slate400; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: @slate300; border-radius: 4px; min-width: 28px; }
QScrollBar::handle:horizontal:hover { background: @slate400; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QScrollArea { border: 0; background: transparent; }
QSplitter::handle { background: @border; }
QProgressBar { background: @slate100; border: 0; border-radius: 3px; height: 6px; text-align: center; color: transparent; }
QProgressBar::chunk { background: @primary; border-radius: 3px; }
QMenu { background: @surface; border: 1px solid @borderStrong; border-radius: 10px; padding: 5px; }
QMenu::item { padding: 7px 18px; border-radius: 6px; }
QMenu::item:selected { background: @leaf50; color: @leaf800; }
QSlider::groove:horizontal { height: 4px; background: @slate200; border-radius: 2px; }
QSlider::sub-page:horizontal { background: @primary; border-radius: 2px; }
QSlider::handle:horizontal { background: @surface; border: 2px solid @primary; width: 12px; height: 12px; margin: -6px 0; border-radius: 7px; }
)");
}

}  // namespace Theme
