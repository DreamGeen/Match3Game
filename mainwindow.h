#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QVBoxLayout>
#include "GamePanel.h"
#include "Global.h"
#include <QVariantAnimation> // 用于实现平滑的数值动画

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // 【修改点】：增加 GameMode mode 参数，以便从大厅接收玩家选择的模式
    explicit MainWindow(UserSession session, GameMode mode, QWidget *parent = nullptr);

private slots:
    void updateScoreLabel(int score);
    void updateComboLabel(int combo); // 接收连击数更新的槽函数

private:
    UserSession m_session;
    GameMode m_currentMode; // 【新增】：保存当前游戏模式
    GamePanel *m_gamePanel;
    GameLogic *m_logic;

    QLabel *m_scoreLabel;
    QLabel *m_infoLabel;
    QLabel *m_comboLabel;

    void setupUI();
};

#endif // MAINWINDOW_H
