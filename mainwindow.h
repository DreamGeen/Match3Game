#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QVBoxLayout>
#include "GamePanel.h"
#include "Global.h"
#include <QVariantAnimation> // 【新增】：用于实现平滑的数值动画

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(UserSession session, QWidget *parent = nullptr);

private slots:
    void updateScoreLabel(int score);

    void updateComboLabel(int combo); // 【新增】：接收连击数更新的槽函数

private:
    UserSession m_session;
    GamePanel *m_gamePanel;
    GameLogic *m_logic;

    QLabel *m_scoreLabel;
    QLabel *m_infoLabel;
    QLabel *m_comboLabel; // <--- 【加上这一行！】

    void setupUI();
};

#endif
