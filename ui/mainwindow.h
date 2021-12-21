#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStateMachine>
#include <QDebug>

#include "screen_recorder.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_captureButton_clicked();

    void on_stopButton_clicked();

    void on_pauseButton_clicked();
    
    void on_resumeButton_clicked();

    void on_checkBox_stateChanged(int arg1);

private:
    Ui::MainWindow *ui;
    ScreenRecorder *sc;

/* 
    QStateMachine *machine;

    QState *s1;
    QState *s2;
    QState *s3; */


};
#endif // MAINWINDOW_H
