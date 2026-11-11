#include "UserTheme.h"

namespace UT {

QString hex(const QColor& c) { return c.name(QColor::HexRgb); }

QFont font(int px, bool bold) {
    QFont f;
    f.setFamilies({"Inter", "Segoe UI", "Roboto", "Noto Sans", "DejaVu Sans"});
    f.setPixelSize(px);
    f.setWeight(bold ? QFont::DemiBold : QFont::Normal);
    return f;
}

QString stylesheet() {
    return DS::applyTokens(R"(
* { font-family: "Inter", "Segoe UI", "Roboto", "Noto Sans", "DejaVu Sans", sans-serif; font-size: 13px; outline: 0; }
QMainWindow, QDialog { background: @slate50; }
QWidget { color: @text; }
QWidget#UserRoot { background: @slate50; }
QStackedWidget { background: transparent; }
QFrame#Col { background: @surface; border: 0; }
QFrame#Notice { background: @leaf50; border: 0; border-bottom: 1px solid @leaf100; }
QFrame#Rule { background: @border; max-height: 1px; min-height: 1px; border: 0; }
QFrame#HCard { background: @surface; border: 1px solid @border; border-radius: 16px; }
QFrame#HCard QLabel { background: transparent; }
QLabel { background: transparent; }
QToolTip { background: @slate900; color: #ffffff; border: 0; padding: 5px 9px; border-radius: 6px; }

QScrollArea { border: 0; background: transparent; }
QScrollArea > QWidget > QWidget { background: transparent; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: @slate300; border-radius: 5px; min-height: 36px; margin: 2px; }
QScrollBar::handle:vertical:hover { background: @slate400; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QLineEdit, QComboBox, QDateEdit {
    background: @surface; border: 1px solid @borderStrong; border-radius: 10px; padding: 8px 12px; color: @text;
    selection-background-color: @leaf100; selection-color: @leaf800; }
QLineEdit:hover, QComboBox:hover, QDateEdit:hover { border-color: @slate400; }
QLineEdit:focus, QComboBox:focus, QDateEdit:focus { border: 1px solid @primary; }
QComboBox::drop-down, QDateEdit::drop-down { border: 0; width: 24px; }
QComboBox::down-arrow, QDateEdit::down-arrow { image: url(:/icons/chevron-down-dark.svg); width: 12px; height: 12px; }
QComboBox QAbstractItemView { background: @surface; border: 1px solid @borderStrong; border-radius: 8px; selection-background-color: @leaf50; selection-color: @leaf800; outline: 0; padding: 4px; }
QCalendarWidget QWidget { background: @surface; color: @text; }
QCalendarWidget QAbstractItemView:enabled { selection-background-color: @primary; selection-color: #ffffff; color: @text; }

QPushButton { background: @surface; border: 1px solid @borderStrong; border-radius: 10px; padding: 9px 18px; color: @slate700; font-weight: 600; }
QPushButton:hover { background: @slate50; border-color: @slate400; color: @text; }
QPushButton:pressed { background: @slate100; }
QPushButton:focus { border: 1px solid @primary; }
QPushButton:disabled { color: @slate400; border-color: @border; background: @slate50; }
QPushButton[ubtn="solid"], QPushButton[ubtn="green"] { background: @primary; border: 1px solid @primary; color: #ffffff; }
QPushButton[ubtn="solid"]:hover, QPushButton[ubtn="green"]:hover { background: @primaryHover; border-color: @primaryHover; color: #ffffff; }
QPushButton[ubtn="solid"]:pressed, QPushButton[ubtn="green"]:pressed { background: @primaryPressed; border-color: @primaryPressed; }
QPushButton[ubtn="solid"]:disabled { background: @leaf200; border-color: @leaf200; color: #ffffff; }
QPushButton[ubtn="small"] { padding: 5px 13px; border-radius: 8px; font-size: 12px; }
QPushButton[ubtn="link"] { background: transparent; border: 0; padding: 4px 2px; color: @primary; font-weight: 600; }
QPushButton[ubtn="link"]:hover { color: @primaryHover; background: transparent; }
QPushButton[ubtn="darkghost"] { background: @surface; border: 1px solid @borderStrong; color: @slate700; padding: 7px 16px; border-radius: 10px; }
QPushButton[ubtn="darkghost"]:hover { border-color: @primary; color: @primary; background: @leaf50; }
QPushButton[ubtn="hero"] { background: #ffffff; border: 1px solid #ffffff; color: @leaf800; padding: 13px 26px; border-radius: 12px; font-size: 14px; font-weight: 700; }
QPushButton[ubtn="hero"]:hover { background: @leaf50; border-color: @leaf50; }
QPushButton[ubtn="heroGhost"] { background: rgba(255,255,255,0.10); border: 1px solid rgba(255,255,255,0.45); color: #ffffff; padding: 13px 26px; border-radius: 12px; font-size: 14px; font-weight: 600; }
QPushButton[ubtn="heroGhost"]:hover { background: rgba(255,255,255,0.20); border-color: #ffffff; }
)");
}

}  // namespace UT
