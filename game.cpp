#include <GLUT/glut.h>
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



const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

enum GameState { MENU, PLAYING, PAUSED, GAME_OVER };
GameState currentState = MENU;

int score = 0;
int lives = 3;
float timeElapsed = 0.0f;
int frameCounter = 0; 

bool keyStates[256] = {false}; 


enum SoundType { SOUND_PADDLE, SOUND_BLOCK, SOUND_PERK, SOUND_LOSE };

static std::queue<SoundType>      audioQueue;
static std::mutex                 audioMutex;
static std::condition_variable    audioCond;
static std::atomic<bool>          audioRunning(true);
static std::thread                audioThread;

static void audioWorker() {
    while (audioRunning.load()) {
        SoundType type;

        {
            std::unique_lock<std::mutex> lock(audioMutex);
            audioCond.wait(lock, []{
                return !audioQueue.empty() || !audioRunning.load();
            });

            if (!audioRunning.load() && audioQueue.empty())
                break;

            type = audioQueue.front();
            audioQueue.pop();
        }

       
        std::cout << '\a' << std::flush;
    }
}

void playSound(SoundType type) {
    {
        std::lock_guard<std::mutex> lock(audioMutex);
        if (audioQueue.size() >= 3 && type != SOUND_LOSE) return;
        audioQueue.push(type);
    }
    audioCond.notify_one();
}


struct Ball {
    float x, y;
    float dx, dy;
    float radius;
    float speedMultiplier;
} ball;

struct Paddle {
    float x, y;
    float width, height;
    float speed; 
} paddle;

struct Block {
    float x, y;
    float width, height;
    bool active;
    int type; 
    float r, g, b;
};
std::vector<Block> blocks;

struct Perk {
    float x, y;
    float width, height;
    float dy;
    int type;
    bool active;
};
std::vector<Perk> activePerks;


void initGame() {
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

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
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
            if (perkChance < 5) b.type = 1; 
            else if (perkChance < 15) b.type = 2; 
            else if (perkChance < 25) b.type = 3; 
            else b.type = 0; 

            blocks.push_back(b);
        }
    }
}

void drawText(float x, float y, std::string text) {
    glRasterPos2f(x, y);
    for (char c : text) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }
}

bool checkCollision(float bx, float by, float br, float rx, float ry, float rw, float rh) {
    float testX = bx;
    float testY = by;

    if (bx < rx) testX = rx;      
    else if (bx > rx + rw) testX = rx + rw;   
    if (by < ry) testY = ry;      
    else if (by > ry + rh) testY = ry + rh;   

    float distX = bx - testX;
    float distY = by - testY;
    float distance = (distX*distX) + (distY*distY);

    return distance <= br*br;
}


#include <chrono>

static auto lastFrameTime = std::chrono::high_resolution_clock::now();
static const double TARGET_MS = 1000.0 / 60.0;

void update(int value);

static void idleLoop() {
    auto now = std::chrono::high_resolution_clock::now();

    double elapsed =
        std::chrono::duration<double, std::milli>(
            now - lastFrameTime).count();

    if (elapsed < TARGET_MS)
        return;

    lastFrameTime = now;
    update(0);
}


void update(int value) {
    if (currentState == PLAYING) {
        frameCounter++;
        timeElapsed = frameCounter / 60.0f;

        if (keyStates['a'] || keyStates['A']) paddle.x -= paddle.speed;
        if (keyStates['d'] || keyStates['D']) paddle.x += paddle.speed;

        if (paddle.x < 0) paddle.x = 0;
        if (paddle.x + paddle.width > WINDOW_WIDTH) paddle.x = WINDOW_WIDTH - paddle.width;
        
        ball.x += ball.dx * ball.speedMultiplier;
        ball.y += ball.dy * ball.speedMultiplier;

        if (ball.x - ball.radius < 0) { 
            ball.x = ball.radius; 
            ball.dx *= -1; 
            playSound(SOUND_PADDLE); 
        }
        if (ball.x + ball.radius > WINDOW_WIDTH) { 
            ball.x = WINDOW_WIDTH - ball.radius; 
            ball.dx *= -1; 
            playSound(SOUND_PADDLE); 
        }
        if (ball.y + ball.radius > WINDOW_HEIGHT) { 
            ball.y = WINDOW_HEIGHT - ball.radius; 
            ball.dy *= -1; 
            playSound(SOUND_PADDLE); 
        }

        if (ball.y - ball.radius < 0) {
            playSound(SOUND_LOSE);
            lives--;
            if (lives <= 0) {
                currentState = GAME_OVER;
            } else {
                ball.x = paddle.x + paddle.width / 2.0f;
                ball.y = paddle.y + paddle.height + ball.radius + 5.0f;
                ball.dy = std::abs(ball.dy); 
                ball.speedMultiplier = 1.0f; 
                paddle.width = 100.0f; 
            }
        }

        if (ball.dy < 0 && checkCollision(ball.x, ball.y, ball.radius, paddle.x, paddle.y, paddle.width, paddle.height)) {
            playSound(SOUND_PADDLE);
            ball.y = paddle.y + paddle.height + ball.radius; 
            
            float hitPoint = ball.x - (paddle.x + paddle.width / 2.0f);
            float normalizedHitPoint = hitPoint / (paddle.width / 2.0f); 
            float currentSpeed = sqrt(ball.dx*ball.dx + ball.dy*ball.dy); 
            
            ball.dx = normalizedHitPoint * currentSpeed * 0.85f;
            ball.dy = sqrt(currentSpeed*currentSpeed - ball.dx*ball.dx); 
        }

        for (auto& b : blocks) {
            if (b.active && checkCollision(ball.x, ball.y, ball.radius, b.x, b.y, b.width, b.height)) {
                b.active = false;
                score += 10;
                playSound(SOUND_BLOCK);
                
                float bcX = b.x + b.width / 2.0f;
                float bcY = b.y + b.height / 2.0f;

                float wy = (b.width + ball.radius * 2) * (ball.y - bcY);
                float hx = (b.height + ball.radius * 2) * (ball.x - bcX);

                if (wy > hx) {
                    if (wy > -hx) ball.dy = std::abs(ball.dy); 
                    else ball.dx = -std::abs(ball.dx);         
                } else {
                    if (wy > -hx) ball.dx = std::abs(ball.dx); 
                    else ball.dy = -std::abs(ball.dy);         
                }
                
                if (b.type != 0) {
                    Perk p = { b.x + b.width/2.0f - 10.0f, b.y, 20.0f, 10.0f, -2.5f, b.type, true };
                    activePerks.push_back(p);
                }
                break; 
            }
        }

        bool won = true;
        for(const auto& b : blocks) if(b.active) won = false;
        if(won) currentState = GAME_OVER;

        for (auto& p : activePerks) {
            if (p.active) {
                p.y += p.dy; 
                if (p.x < paddle.x + paddle.width && p.x + p.width > paddle.x &&
                    p.y < paddle.y + paddle.height && p.y + p.height > paddle.y) {
                    
                    p.active = false;
                    score += 50;
                    playSound(SOUND_PERK);
                    
                    if (p.type == 1) lives++;
                    else if (p.type == 2) ball.speedMultiplier += 0.15f;
                    else if (p.type == 3) paddle.width = std::min(paddle.width + 30.0f, (float)WINDOW_WIDTH / 2.0f);
                }
                if (p.y < 0) p.active = false;
            }
        }

        activePerks.erase(
            std::remove_if(activePerks.begin(), activePerks.end(),
                [](const Perk& p){ return !p.active; }),
            activePerks.end()
        );
    }

    glutPostRedisplay();
}

// Subrina's part start from here
void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (currentState == MENU) {
        glColor3f(1.0f, 1.0f, 1.0f);
        drawText(WINDOW_WIDTH/2 - 100, WINDOW_HEIGHT/2 + 20, "DX BALL CLONE");
        drawText(WINDOW_WIDTH/2 - 120, WINDOW_HEIGHT/2 - 20, "Press 'S' to Start Game");
        drawText(WINDOW_WIDTH/2 - 120, WINDOW_HEIGHT/2 - 50, "Press 'ESC' to Exit");
    } 
    else if (currentState == PLAYING || currentState == PAUSED) {
        glColor3f(0.8f, 0.8f, 0.8f);
        glRectf(paddle.x, paddle.y, paddle.x + paddle.width, paddle.y + paddle.height);

        glColor3f(1.0f, 0.0f, 0.0f);
        glBegin(GL_POLYGON);
        for(int i = 0; i < 360; i += 10) {
            float theta = i * 3.14159f / 180.0f;
            glVertex2f(ball.x + ball.radius * cos(theta), ball.y + ball.radius * sin(theta));
        }
        glEnd();

        for (const auto& b : blocks) {
            if (b.active) {
                glColor3f(b.r, b.g, b.b);
                glRectf(b.x, b.y, b.x + b.width, b.y + b.height);
                glColor3f(0.0f, 0.0f, 0.0f);
                glBegin(GL_LINE_LOOP);
                glVertex2f(b.x, b.y); glVertex2f(b.x + b.width, b.y);
                glVertex2f(b.x + b.width, b.y + b.height); glVertex2f(b.x, b.y + b.height);
                glEnd();
            }
        }

        for (const auto& p : activePerks) {
            if (p.active) {
                if (p.type == 1) glColor3f(1.0f, 0.0f, 1.0f); 
                else if (p.type == 2) glColor3f(1.0f, 1.0f, 0.0f); 
                else glColor3f(0.0f, 1.0f, 1.0f); 
                glRectf(p.x, p.y, p.x + p.width, p.y + p.height);
            }
        }

        glColor3f(1.0f, 1.0f, 1.0f);
        std::stringstream uiText;
        uiText << "Score: " << score << "   Lives: " << lives << "   Time: " << (int)timeElapsed << "s";
        drawText(10.0f, WINDOW_HEIGHT - 25.0f, uiText.str());

        if (currentState == PAUSED) {
            drawText(WINDOW_WIDTH/2 - 50, WINDOW_HEIGHT/2, "PAUSED");
            drawText(WINDOW_WIDTH/2 - 100, WINDOW_HEIGHT/2 - 30, "Press 'P' to Resume");
        }
    }
    else if (currentState == GAME_OVER) {
        glColor3f(1.0f, 0.0f, 0.0f);
        if (lives > 0) drawText(WINDOW_WIDTH/2 - 50, WINDOW_HEIGHT/2, "YOU WIN!");
        else drawText(WINDOW_WIDTH/2 - 50, WINDOW_HEIGHT/2, "GAME OVER");
        
        std::stringstream fScore; fScore << "Final Score: " << score;
        drawText(WINDOW_WIDTH/2 - 60, WINDOW_HEIGHT/2 - 30, fScore.str());
        drawText(WINDOW_WIDTH/2 - 110, WINDOW_HEIGHT/2 - 60, "Press 'M' for Main Menu");
    }

    glutSwapBuffers();
}

void keyboardDown(unsigned char key, int x, int y) {
    keyStates[key] = true; 
    if (key == 27) exit(0); 

    if (currentState == MENU && (key == 's' || key == 'S')) {
        initGame();
        currentState = PLAYING;
    }
    else if (currentState == PLAYING && (key == 'p' || key == 'P')) {
        currentState = PAUSED;
    }
    else if (currentState == PAUSED && (key == 'p' || key == 'P')) {
        currentState = PLAYING;
    }
    else if (currentState == GAME_OVER && (key == 'm' || key == 'M')) {
        currentState = MENU;
    }
}

void keyboardUp(unsigned char key, int x, int y) {
    keyStates[key] = false; 
}

void mouseMotion(int x, int y) {
    if (currentState == PLAYING) {
        paddle.x = x - paddle.width / 2.0f;
        if (paddle.x < 0) paddle.x = 0;
        if (paddle.x + paddle.width > WINDOW_WIDTH) paddle.x = WINDOW_WIDTH - paddle.width;
    }
}

int main(int argc, char** argv) {
    srand(time(0));

    

    audioThread = std::thread(audioWorker);

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("DX Ball C++ OpenGL");

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT);

    glutDisplayFunc(display);
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp); 
    glutPassiveMotionFunc(mouseMotion); 
    
    
    glutIdleFunc(idleLoop);

    initGame();
    glutMainLoop();

    audioRunning.store(false);
    audioCond.notify_all();
    if (audioThread.joinable()) audioThread.join();

    return 0;
}
