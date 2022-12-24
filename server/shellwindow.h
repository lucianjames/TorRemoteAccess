#ifndef SHELLWINDOW_H
#define SHELLWINDOW_H

#include <QMainWindow>
#include <QListView>
#include <QStringListModel>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <vector>


QT_BEGIN_NAMESPACE
namespace Ui { class ShellWindow; }
QT_END_NAMESPACE

class ShellWindow : public QWidget{
    Q_OBJECT

private:

public:
    ShellWindow(QWidget *parent = nullptr);
    ~ShellWindow();

private slots: // Slots are functions that are called when a signal is emitted
    void helloWorld();

signals:

private:
    Ui::ShellWindow *ui;
};

#endif // SHELLWINDOW_H