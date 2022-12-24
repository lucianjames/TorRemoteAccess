#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListView>
#include <QStringListModel>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <vector>

struct Connection{
    int fd;
    std::string publicIp;
    std::string username;
    std::string hostname;
    Connection(int fd, std::string publicIp, std::string username, std::string hostname){
        this->fd = fd;
        this->publicIp = publicIp;
        this->username = username;
        this->hostname = hostname;
    }
};

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow{
    Q_OBJECT

private:
    int fd; // File descriptor for the socket
    unsigned int port = 8080; // Port to listen on
    struct sockaddr_in serverAddress; // Server address

    std::vector<Connection> connections; // List of connections
    QStringListModel connectionsListViewModel;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots: // Slots are functions that are called when a signal is emitted
    void startServer(); // Starts the server
    void listenForConnections(); // Listens for incoming connections, spawns threads to handle them
    void handleNewConnection(int newConnFd); // Handles a new connection
    void updateConnectionsListView(); // Updates the connections list view based on the connections vector
    void logAppend(QString text); // Appends text to the logTextBrowser
    void checkConnections(); // Checks if any connections have disconnected by sending a "ping", (expects a "pong" back)
    void activateShell(int fd); // Activates the shell for the connection with the given fd
    void helloWorld();

signals:
    void logAppendSignal(QString text); // Signal to append text to the logTextBrowser

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
