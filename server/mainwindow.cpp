#include <QFuture>
#include <QtConcurrent/QtConcurrent>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "shellwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow){ // Constructor
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icon.ico"));
    connect(this, &MainWindow::logAppendSignal, this, &MainWindow::logAppend);

    // Run this->checkConnections() every second
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::checkConnections);
    timer->start(1000);

    this->connectionsListViewModel.setStringList(QStringList());
    ui->connectionsListView->setModel(&this->connectionsListViewModel);
    ui->connectionsListView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Run this->activateShell() when a connection is double clicked. Get the fd from the connections vector
    connect(ui->connectionsListView, &QListView::doubleClicked, this, [=](const QModelIndex &index){
        this->activateShell(this->connections[index.row()].fd);
    });

    // Initialise the logTextBrowser

    this->startServer();
}

MainWindow::~MainWindow(){
    printf("~MainWindow() called\n");
    for(int i = 0; i < this->connections.size(); i++){
        int closeResult = ::close(this->connections[i].fd);
        printf("Closed connection %d with fd %d. close() returned %d\n", i, this->connections[i].fd, closeResult);
    }
    int closeResult = ::close(this->fd);
    printf("Closed server socket with fd %d. close() returned %d\n", this->fd, closeResult);
    delete ui;
    exit(0); // Exit the program when the window is closed
}

void MainWindow::startServer(){
    this->fd = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
    if(this->fd == -1){
        ui->logTextBrowser->append("Failed to create socket. perror: " + QString::fromStdString(strerror(errno)));
        return;
    }
    ui->logTextBrowser->append("Socket created");
    int opt = 1;
    if(setsockopt(this->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        ui->logTextBrowser->append("Failed to set socket options. perror: " + QString::fromStdString(strerror(errno)));
        return;
    }
    ui->logTextBrowser->append("Socket options set");
    this->serverAddress.sin_family = AF_INET;
    this->serverAddress.sin_addr.s_addr = INADDR_ANY;
    this->serverAddress.sin_port = htons(this->port);
    if(bind(this->fd, (struct sockaddr *)&this->serverAddress, sizeof(this->serverAddress)) < 0){
        ui->logTextBrowser->append("Failed to bind socket. perror: " + QString::fromStdString(strerror(errno)));
        return;
    }
    ui->logTextBrowser->append("Socket bound");
    if(listen(this->fd, 3) < 0){
        ui->logTextBrowser->append("Failed to listen on socket. perror: " + QString::fromStdString(strerror(errno)));
        return;
    }
    ui->logTextBrowser->append("Listening for connections...");
    // Spawn a thread to listen for connections
    QFuture<void> future = QtConcurrent::run(this, &MainWindow::listenForConnections);
}

void MainWindow::listenForConnections(){
    int addrlen = sizeof(this->serverAddress);
    while(true){
        int newConnFd = accept(this->fd, (struct sockaddr *)&this->serverAddress, (socklen_t*)&addrlen);
        if(newConnFd < 0){
            emit logAppendSignal("Failed to accept a connection. perror: " + QString::fromStdString(strerror(errno)));
        }else{
            emit logAppendSignal("New connection accepted, fd: " + QString::number(newConnFd));
            // Spawn a thread to handle the new connection (to ensure that listening for connections is not blocked)
            QFuture<void> future = QtConcurrent::run(this, &MainWindow::handleNewConnection, newConnFd);
        }
    }
}

void MainWindow::handleNewConnection(int newConnFd){
    /*
        Connection message format:
            <ip>;<username>;<hostname>;
    */
    char buffer[1024] = {0};
    int bytesRead = read(newConnFd, buffer, 1024);
    std::string connMsg(buffer); // Convert the buffer to a string for easier handling
    if(std::count(connMsg.begin(), connMsg.end(), ';') != 3){
        emit logAppendSignal("Invalid connection message from fd: " + QString::number(newConnFd));
        return;
    }
    std::string ip = connMsg.substr(0, connMsg.find(';'));
    connMsg.erase(0, connMsg.find(';') + 1); // Erase the ip from the string. <ip>;<username>;<hostname>;  --->  <username>;<hostname>;
    std::string username = connMsg.substr(0, connMsg.find(';'));
    connMsg.erase(0, connMsg.find(';') + 1); // Erase the username from the string. <username>;<hostname>;  --->  <hostname>;
    std::string hostname = connMsg.substr(0, connMsg.find(';'));
    connMsg.erase(0, connMsg.find(';') + 1); // Erase the hostname from the string. <hostname>;  --->  ;
    emit logAppendSignal("ip: " + QString::fromStdString(ip) + ", username: " + QString::fromStdString(username) + ", hostname: " + QString::fromStdString(hostname));

    // Add the client to the connections vector
    Connection newConnection(newConnFd, ip, username, hostname);
    this->connections.push_back(newConnection);
    // Update the connections list view
    this->updateConnectionsListView();

    // Send a message to the client to confirm the connection
    /*
        Confirmation message format:
            <ip>;<username>;<hostname>;connected;
    */
    std::string confMsg = ip + ";" + username + ";" + hostname + ";connected;";
    send(newConnFd, confMsg.c_str(), confMsg.length(), 0);
}

void MainWindow::updateConnectionsListView(){
    QStringList connectionsList;
    for(Connection conn : this->connections){
        connectionsList.append(QString::fromStdString(conn.publicIp + " - " + conn.username + "@" + conn.hostname + " - fd: " + std::to_string(conn.fd)));
    }
    this->connectionsListViewModel.setStringList(connectionsList);
}

void MainWindow::logAppend(QString msg){
    ui->logTextBrowser->append(msg);
}

void MainWindow::checkConnections(){
    for(int i = 0; i < this->connections.size(); i++){
        // Send a "ping" message to the client
        /*
            Ping message format:
                ping;
        */
        std::string pingMsg = "ping;";
        send(this->connections[i].fd, pingMsg.c_str(), pingMsg.length(), 0);
        // Read the response (expecting "ping;pong;")
        char buffer[1024] = {0};
        int bytesRead = read(this->connections[i].fd, buffer, 1024);
        std::string pongMsg(buffer);
        if(pongMsg != "ping;pong;"){
            // The client did not respond with the expected message
            emit logAppendSignal("Client with fd: " + QString::number(this->connections[i].fd) + " did not respond to ping. Removing client from connections list.");
            // Remove the client from the connections list
            this->connections.erase(this->connections.begin() + i);
            // Update the connections list view
            this->updateConnectionsListView();
        }
    }
}

void MainWindow::activateShell(int fd){
    // Spawn a shellwindow
    ShellWindow *shellWindow = new ShellWindow(fd);
    shellWindow->show();
}


void MainWindow::helloWorld(){
    ui->logTextBrowser->append("Hello World!");
}