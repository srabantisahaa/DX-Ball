#include <windows.h>
#include <mmsystem.h>
#include <GL/glut.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#pragma comment(lib, "winmm.lib")

// --- Constants & Global Variables ---
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

enum GameState
{
    MENU,
    PLAYING,
    PAUSED,
    GAME_OVER
};
GameState currentState = MENU;

int score = 0;
int lives = 3;
float timeElapsed = 0.0f;
int frameCounter = 0; // Used to track elapsed seconds smoothly

bool keyStates[256] = {false};

// --- Threaded Audio System ---
enum SoundType
{
    SOUND_PADDLE,
    SOUND_BLOCK,
    SOUND_PERK,
    SOUND_LOSE
};

static std::queue<SoundType> audioQueue;
static std::mutex audioMutex;
static std::condition_variable audioCond;
static std::atomic<bool> audioRunning(true);
static std::thread audioThread;

static void audioWorker()
{
    while (audioRunning.load())
    {
        SoundType type;
        {
            std::unique_lock<std::mutex> lock(audioMutex);
            audioCond.wait(lock, []
                           { return !audioQueue.empty() || !audioRunning.load(); });
            if (!audioRunning.load() && audioQueue.empty())
                break;
            type = audioQueue.front();
            audioQueue.pop();
        }
        switch (type)
        {
        case SOUND_PADDLE:
            PlaySoundA("SystemStart", NULL, SND_ALIAS | SND_SYNC | SND_NODEFAULT);
            break;
        case SOUND_BLOCK:
            PlaySoundA("SystemAsterisk", NULL, SND_ALIAS | SND_SYNC | SND_NODEFAULT);
            break;
        case SOUND_PERK:
            PlaySoundA("SystemExclamation", NULL, SND_ALIAS | SND_SYNC | SND_NODEFAULT);
            break;
        case SOUND_LOSE:
            PlaySoundA("SystemHand", NULL, SND_ALIAS | SND_SYNC | SND_NODEFAULT);
            break;
        }
    }
}

void playSound(SoundType type)
{
    {
        std::lock_guard<std::mutex> lock(audioMutex);
        if (audioQueue.size() >= 3 && type != SOUND_LOSE)
            return;
        audioQueue.push(type);
    }
    audioCond.notify_one();
}

// --- Game Objects ---
struct Ball
{
    float x, y;
    float dx, dy;
    float radius;
    float speedMultiplier;
} ball;

struct Paddle
{
    float x, y;
    float width, height;
    float speed;
} paddle;

struct Block
{
    float x, y;
    float width, height;
    bool active;
    int type;
    float r, g, b;
};
std::vector<Block> blocks;

struct Perk
{
    float x, y;
    float width, height;
    float dy;
    int type;
    bool active;
};
std::vector<Perk> activePerks;

// --- Initialization ---
void initGame()
{
    score = 0;
    lives = 3;
    timeElapsed = 0.0f;
    frameCounter = 0;

    paddle.width = 100.0f;
    paddle.height = 15.0f;
    paddle.x = WINDOW_WIDTH / 2.0f - paddle.width / 2.0f;
    paddle.y = 30.0f;
    paddle.speed = 10.0f;

    ball.radius = 8.0f;
    ball.x = paddle.x + paddle.width / 2.0f;
    ball.y = paddle.y + paddle.height + ball.radius;
    ball.dx = 4.5f;
    ball.dy = 4.5f;
    ball.speedMultiplier = 1.0f;

    blocks.clear();
    activePerks.clear();
    int rows = 6;
    int cols = 10;
    float blockWidth = 70.0f;
    float blockHeight = 20.0f;
    float startX = 45.0f;
    float startY = 400.0f;

    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            Block b;
            b.x = startX + j * (blockWidth + 2);
            b.y = startY + i * (blockHeight + 2);
            b.width = blockWidth;
            b.height = blockHeight;
            b.active = true;

            b.r = (float)(rand() % 100) / 100.0f;
            b.g = (float)(rand() % 100) / 100.0f;
            b.b = (float)(rand() % 100) / 100.0f + 0.5f;

            int perkChance = rand() % 100;
            if (perkChance < 5)
                b.type = 1;
            else if (perkChance < 15)
                b.type = 2;
            else if (perkChance < 25)
                b.type = 3;
            else
                b.type = 0;

            blocks.push_back(b);
        }
    }
}

// --- Utility Functions ---
void drawText(float x, float y, std::string text)
{
    glRasterPos2f(x, y);
    for (char c : text)
    {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }
}

bool checkCollision(float bx, float by, float br, float rx, float ry, float rw, float rh)
{
    float testX = bx;
    float testY = by;

    if (bx < rx)
        testX = rx;
    else if (bx > rx + rw)
        testX = rx + rw;
    if (by < ry)
        testY = ry;
    else if (by > ry + rh)
        testY = ry + rh;

    float distX = bx - testX;
    float distY = by - testY;
    float distance = (distX * distX) + (distY * distY);

    return distance <= br * br;
}
