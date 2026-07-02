#pragma once
#include <atomic>
#include <thread>
#include "shared_state.h"

// =====================================================================
// Поток компьютерного зрения.
//
// ЭТО ЗАГЛУШКА. Сюда подключается реальный детектор метки
// (OpenCV / AprilTag / ArUco). Задача потока — заполнять VisionShared:
//   ex, ey   — смещение центра метки от центра кадра, пиксели
//   detected — найдена ли метка в текущем кадре
//
// Чтобы проект собирался без зависимостей, по умолчанию метка
// "не обнаружена". См. VisionThread::run().
// =====================================================================

class VisionThread {
public:
    explicit VisionThread(VisionShared& out) : out_(out) {}

    void start();
    void stop();

private:
    void run();

    VisionShared& out_;
    std::thread th_;
    std::atomic<bool> running_{false};
};
