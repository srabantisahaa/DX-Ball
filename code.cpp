// --- High-Resolution Frame Timing ---
static LARGE_INTEGER qpcFreq   = {};
static LARGE_INTEGER qpcLast   = {};
static const double  TARGET_MS = 1000.0 / 60.0; // 16.6667ms

void update(int value); // forward declare

static void idleLoop() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double elapsed = (double)(now.QuadPart - qpcLast.QuadPart)
                     / (double)qpcFreq.QuadPart * 1000.0;
    if (elapsed < TARGET_MS) return;
    qpcLast = now;
    update(0);
}

// --- Logic Update ---
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
