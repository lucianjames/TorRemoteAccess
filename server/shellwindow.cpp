#include <QFuture>
#include <QtConcurrent/QtConcurrent>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "shellwindow.h"
#include "./ui_shellwindow.h"


ShellWindow::ShellWindow(QWidget *parent) : QWidget(parent), ui(new Ui::ShellWindow){ // Constructor
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icon.ico"));
    // Make it so that when enter is pressed on the lineEdit, hello world is printed
    connect(ui->lineEdit, &QLineEdit::returnPressed, this, &ShellWindow::helloWorld);
}

ShellWindow::~ShellWindow(){
    delete ui;
}

void ShellWindow::helloWorld(){
    ui->textBrowser->append("Hello world");
}
