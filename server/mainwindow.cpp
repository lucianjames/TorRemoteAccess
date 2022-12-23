#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow){ // Constructor
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icon.ico"));
    connect(ui->openShellButton, &QPushButton::clicked, this, &MainWindow::helloWorld);
}

MainWindow::~MainWindow(){
    delete ui;
}

void MainWindow::helloWorld(){
    ui->shellTextBrowser->append("Hello World!");
}