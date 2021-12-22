#include "mainwindow.h"

#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , sc(new ScreenRecorder)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete sc;
    delete ui;
}

void MainWindow::on_captureButton_clicked()
{
    QString output_file = ui->lineEdit_filename->text();
    QString framerate = ui->lineEdit_framerate->text();
    bool capture_audio = ui->checkBox->checkState() == Qt::Unchecked ? false : true;

    ui->stopButton->setProperty("enabled", true);
    ui->pauseButton->setProperty("enabled", true);
    ui->captureButton->setProperty("enabled", false);

    try {
        sc->start(":0.0+","hw:0,0",output_file.toStdString(),264,264,0,0,framerate.toInt());
    } catch (const std::exception &e) {
                //std::cerr << e.what() << ", terminating..." << std::endl;
                exit(1);
    }
}

void MainWindow::on_stopButton_clicked()
{
    try {
        sc->stop();
    } catch (const std::exception &e) {
                //std::cerr << e.what() << ", terminating..." << std::endl;
                exit(1);
    }

    ui->captureButton->setProperty("enabled", true);
    ui->stopButton->setProperty("enabled", false);
    ui->pauseButton->setProperty("enabled", false);
    
}

void MainWindow::on_pauseButton_clicked()
{
    try {
        sc->pause();
    } catch (const std::exception &e) {
                //std::cerr << e.what() << ", terminating..." << std::endl;
                exit(1);
    }

    ui->pauseButton->setProperty("enabled", false);
    ui->stopButton->setProperty("enabled", true);
    ui->resumeButton->setProperty("enabled", true);
}

void MainWindow::on_resumeButton_clicked()
{
    try {
        sc->resume();
    } catch (const std::exception &e) {
                //std::cerr << e.what() << ", terminating..." << std::endl;
                exit(1);
    }

    ui->pauseButton->setProperty("enabled", true);
    ui->resumeButton->setProperty("enabled", false);
}

void MainWindow::on_checkBox_stateChanged(int arg1)
{
    if (arg1 == 2){
        ui->lineEdit_audioDevice->setProperty("enabled", true);
    }else{
        ui->lineEdit_audioDevice->setProperty("enabled", false);
    }
}