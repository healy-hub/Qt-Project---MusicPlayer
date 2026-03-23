#ifndef MOREMENU_H
#define MOREMENU_H

#include <QWidget>

namespace Ui {
class MoreMenu;
}

class MoreMenu : public QWidget
{
    Q_OBJECT

public:
    explicit MoreMenu(QWidget *parent = nullptr);
    ~MoreMenu();

signals:
    void addMusicClicked();
    void setMusicDirClicked();

private:
    Ui::MoreMenu *ui;
};

#endif // MOREMENU_H
