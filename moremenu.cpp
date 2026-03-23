#include "moremenu.h"
#include "ui_moremenu.h"

MoreMenu::MoreMenu(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MoreMenu)
{
    ui->setupUi(this);
    setFixedSize(150, 80);

    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(false);

    connect(ui->addMusic, &QPushButton::clicked, this, &MoreMenu::addMusicClicked);
    connect(ui->setMusicDir, &QPushButton::clicked, this, &MoreMenu::setMusicDirClicked);
}

MoreMenu::~MoreMenu()
{
    delete ui;
}
