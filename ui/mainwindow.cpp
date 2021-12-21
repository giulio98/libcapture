#include "mainwindow.h"

#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
/* 
    machine = new QStateMachine(this);
    s1 = new QState();
    s1->assignProperty(ui->captureButton, "text", "capture");
    s1->assignProperty(ui->captureButton,"visible", true);
    s1->assignProperty(ui->stopButton,"visible", false);

    s2 = new QState();
    s2->assignProperty(ui->stopButton, "text", "stop");
    s2->assignProperty(ui->captureButton,"visible", false);
    s2->assignProperty(ui->stopButton,"visible", true);

    s1->addTransition(ui->captureButton, &QAbstractButton::clicked, s2);
    s2->addTransition(ui->stopButton, &QAbstractButton::clicked, s1);

    machine->addState(s1);
    machine->addState(s2);

    machine->setInitialState(s1);
    machine->start(); */
    
}

MainWindow::~MainWindow()
{
    delete ui;
/*     delete machine;
 */}

void MainWindow::on_captureButton_clicked()
{
    sc = new ScreenRecorder;
    QString output_file = ui->lineEdit_filename->text();
    QString framerate = ui->lineEdit_framerate->text();
    bool capture_audio = ui->checkBox->checkState() == Qt::Unchecked ? false : true;

    sc->start(output_file.toStdString(), framerate.toInt(), capture_audio);

    ui->stopButton->setProperty("enabled", true);
    ui->pauseButton->setProperty("enabled", true);
    ui->captureButton->setProperty("enabled", false);

}

void MainWindow::on_stopButton_clicked()
{
    sc->stop();
    delete sc;

    ui->captureButton->setProperty("enabled", true);
    ui->stopButton->setProperty("enabled", false);
    ui->pauseButton->setProperty("enabled", false);
    
}

void MainWindow::on_pauseButton_clicked()
{
    sc->pause();

    ui->pauseButton->setProperty("enabled", false);
    ui->stopButton->setProperty("enabled", true);
    ui->resumeButton->setProperty("enabled", true);
}

void MainWindow::on_resumeButton_clicked()
{
    sc->resume();

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
    
