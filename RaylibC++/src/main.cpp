#include "raylib.h"
#include "Math.h"
#include <cmath> // Required for sqrtf

// Custom Vector2Distance function
float Vector2Distance(Vector2 v1, Vector2 v2)
{
    return sqrtf((v2.x - v1.x) * (v2.x - v1.x) + (v2.y - v1.y) * (v2.y - v1.y));
}
bool Vector2Equals(Vector2 v1, Vector2 v2)
{
    return (v1.x == v2.x && v1.y == v2.y);
}



#include <cassert>
#include <array>
#include <vector>
#include <algorithm>

const float SCREEN_SIZE = 800;

const int TILE_COUNT = 20;
const float TILE_SIZE = SCREEN_SIZE / TILE_COUNT;
const int MAX_TURRETS = 5; // Limit the number of turrets to 5
int turretCount = 0;       // Tracks how many turrets have been placed

int enemyCount = 0;      // Keeps track of the number of spawned enemies
float spawnTimer = 0.0f; // Timer for enemy spawning



enum TileType : int
{
    GRASS,      // Marks unoccupied space, can be overwritten 
    DIRT,       // Marks the path, cannot be overwritten
    WAYPOINT,   // Marks where the path turns, cannot be overwritten
    TURRET,    // New Turret Tile
    COUNT
};

enum GameState
{
    SETUP,   // Placing turrets before the level starts
    PLAYING  // The level is currently running
};
GameState gameState = SETUP; // Start in setup mode


struct Cell
{
    int row;
    int col;
};

constexpr std::array<Cell, 4> DIRECTIONS{ Cell{ -1, 0 }, Cell{ 1, 0 }, Cell{ 0, -1 }, Cell{ 0, 1 } };

inline bool InBounds(Cell cell, int rows = TILE_COUNT, int cols = TILE_COUNT)
{
    return cell.col >= 0 && cell.col < cols && cell.row >= 0 && cell.row < rows;
}

void DrawTile(int row, int col, Color color)
{
    DrawRectangle(col * TILE_SIZE, row * TILE_SIZE, TILE_SIZE, TILE_SIZE, color);
}

void DrawTile(int row, int col, int type)
{
    Color color = type > 0 ? BEIGE : GREEN;
    DrawTile(row, col, color);
}

Vector2 TileCenter(int row, int col)
{
    float x = col * TILE_SIZE + TILE_SIZE * 0.5f;
    float y = row * TILE_SIZE + TILE_SIZE * 0.5f;
    return { x, y };
}

Vector2 TileCorner(int row, int col)
{
    float x = col * TILE_SIZE;
    float y = row * TILE_SIZE;
    return { x, y };
}

// Returns a collection of adjacent cells that match the search value.
std::vector<Cell> FloodFill(Cell start, int tiles[TILE_COUNT][TILE_COUNT], TileType searchValue)
{
    // "open" = "places we want to search", "closed" = "places we've already searched".
    std::vector<Cell> result;
    std::vector<Cell> open;
    bool closed[TILE_COUNT][TILE_COUNT];
    for (int row = 0; row < TILE_COUNT; row++)
    {
        for (int col = 0; col < TILE_COUNT; col++)
        {
            // We don't want to search zero-tiles, so add them to closed!
            closed[row][col] = tiles[row][col] == 0;
        }
    }

    // Add the starting cell to the exploration queue & search till there's nothing left!
    open.push_back(start);
    while (!open.empty())
    {
        // Remove from queue and prevent revisiting
        Cell cell = open.back();
        open.pop_back();
        closed[cell.row][cell.col] = true;

        // Add to result if explored cell has the desired value
        if (tiles[cell.row][cell.col] == searchValue)
            result.push_back(cell);

        // Search neighbours
        for (Cell dir : DIRECTIONS)
        {
            Cell adj = { cell.row + dir.row, cell.col + dir.col };
            if (InBounds(adj) && !closed[adj.row][adj.col] && tiles[adj.row][adj.col] > 0)
                open.push_back(adj);
        }
    }

    return result;
}

struct Enemy
{
    Vector2 position;  // Current position of the enemy
    float speed;       // Movement speed of the enemy
    int currentWaypoint; // Index of the current waypoint the enemy is heading toward
    float health;      // Enemy health
    float radius;      // For rendering and collision detection
    bool active;       // Whether the enemy is still alive
};

struct Turret
{
    Vector2 position;     // Position of the turret
    float range;          // How far the turret can shoot
    float fireRate;       // Time between shots
    float reloadTime;     // Tracks time since last shot
    float bulletSpeed;    // Speed of bullets fired
    float damage;         // How much damage the turret does per shot
    bool active;          // Whether the turret is active
};


struct Bullet
{
    Vector2 position{};
    Vector2 direction{};
    float time = 0.0f;
    bool enabled = true;
};
std::vector<Enemy> enemies; // Collection to store all enemies
std::vector<Bullet> bullets; // Collection to store all bullets
std::vector<Turret> turrets; // Collection to store all turrets

int currentLevel = 1;   // Tracks which level you're on
int enemyHealth = 100;  // Adjust enemy health per level
float spawnInterval = 1.5f;  // Adjust spawn rate per level

void SetupLevel(int level) {
    switch (level) {
    case 1:
        enemyHealth = 100;
        spawnInterval = 1.5f;  // Slower spawn rate
        // Set the initial path waypoints, turrets, etc. for level 1
        // You might want to adjust your tile grid (tiles[][]) for each level
        break;
    case 2:
        enemyHealth = 200;
        spawnInterval = 1.0f;  // Moderate spawn rate
        // Adjust the path and turret layout for level 2
        break;
    case 3:
        enemyHealth = 300;
        spawnInterval = 0.75f; // Faster spawn rate
        // Adjust path, turrets, and other settings for level 3
        break;
    }

    enemies.clear();
    bullets.clear();
    turrets.clear();
    enemyCount = 0;  // Reset enemy count
    spawnTimer = 0.0f;  // Reset spawn timer
}
void AdvanceLevel() {
    if (currentLevel < 3) {
        currentLevel++;
    }
    else {
        currentLevel = 1;  // After level 3, go back to level 1
    }

    SetupLevel(currentLevel);  // Configure the next level
}



int main()
{
    // TODO - Modify this grid to contain turret tiles. Instantiate turret objects accordingly
    int tiles[TILE_COUNT][TILE_COUNT]
    {
        //col:0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19    row:
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0 }, // 0
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 1
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 2
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 3
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 4
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 5
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 6
            { 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0 }, // 7
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 8
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 9
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 10
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 11
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 12
            { 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0 }, // 13
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 }, // 14
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 }, // 15
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 }, // 16
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0 }, // 17
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 18
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }  // 19
    };

    std::vector<Cell> waypoints = FloodFill({ 0, 12 }, tiles, WAYPOINT);
    size_t curr = 0;
    size_t next = curr + 1;
    bool atEnd = false;

    Vector2 enemyPosition = TileCenter(waypoints[curr].row, waypoints[curr].col);
    const float enemySpeed = 250.0f;
    const float enemyRadius = 20.0f;

    const float bulletTime = 1.0f;
    const float bulletSpeed = 500.0f;
    const float bulletRadius = 15.0f;

    std::vector<Bullet> bullets;
    float shootCurrent = 0.0f;
    float shootTotal = 0.25f;

    std::vector<Enemy> enemies; // Collection to store all enemies
    float spawnTimer = 0.0f;
    float spawnInterval = 1.0f;
    int enemyCount = 0;
    int maxEnemies = 10;

    std::vector<Turret> turrets; // Collection to store all turrets
    for (int row = 0; row < TILE_COUNT; row++)
    {
        for (int col = 0; col < TILE_COUNT; col++)
        {
            if (tiles[row][col] == TURRET) // If it's a turret tile
            {
                // Create a turret and  its position and attributes
                Turret turret;
                turret.position = TileCenter(row, col); // Position at tile center
                turret.range = 250.0f;
                turret.fireRate = 0.8f;
                turret.reloadTime = 0.0f;
                turret.bulletSpeed = 500.0f;
                turret.damage = 15.0f;
                turret.active = true;

                turrets.push_back(turret);
            }
        }
    }


    SetupLevel(currentLevel);  // Set up the first level

    {
        // Initialize game resources
        InitWindow(SCREEN_SIZE, SCREEN_SIZE, "Tower Defense");
        SetTargetFPS(60);
        InitAudioDevice();  // Initialize the audio device

    // Load sound files for turret placement and removal
        Sound placeturretSound = LoadSound("../sounds/place_turret.wav");
        Sound removeTurretSound = LoadSound("../sounds/remove_turret.wav");

    SetMasterVolume(1.0f); // Set volume to 100%
        // Start in setup mode
        GameState gameState = SETUP;
        SetupLevel(currentLevel); // Setup the first level

        while (!WindowShouldClose())
        {
            float dt = GetFrameTime(); // Time elapsed for smooth animation/rendering

            // Handle setup mode where player can place/remove turrets
            if (gameState == SETUP)
            {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    PlaySound(placeturretSound);
                    Vector2 mousePosition = GetMousePosition();
                    int col = mousePosition.x / TILE_SIZE;
                    int row = mousePosition.y / TILE_SIZE;

                    if (InBounds({ row, col }))
                    {
                        // Place a turret if it's a GRASS tile and we haven't reached the max limit
                        if (tiles[row][col] == GRASS && turretCount < MAX_TURRETS)
                        {
                            tiles[row][col] = TURRET;

                            // Create a turret and set its properties
                            Turret turret;
                            turret.position = TileCenter(row, col);
                            turret.range = 250.0f;
                            turret.fireRate = 0.8f;
                            turret.reloadTime = 0.0f;
                            turret.bulletSpeed = 500.0f;
                            turret.damage = 15.0f;
                            turret.active = true;

                            turrets.push_back(turret);
                            turretCount++; // Increment the turret counter

                            // Play sound for placing turret
                            PlaySound(placeturretSound);
                        }
                        // Remove a turret if it's already there
                        else if (tiles[row][col] == TURRET)
                        {
                            tiles[row][col] = GRASS;

                            // Remove the turret from the vector
                            auto it = std::remove_if(turrets.begin(), turrets.end(),
                                [&](Turret& turret) {
                                    return Vector2Equals(turret.position, TileCenter(row, col));
                                });
                            turrets.erase(it, turrets.end());
                            turretCount--; // Decrement the turret counter

                            // Play sound for removing turret
                            PlaySound(removeTurretSound);
                        }
                    }
                }

                // If the player presses space, start the game
                if (IsKeyPressed(KEY_SPACE))
                {
                    gameState = PLAYING; // Switch to the game-playing mode
                }
            }



            // Handle gameplay logic (playing mode)
            if (gameState == PLAYING)
            {
                // Enemy spawning, movement, turret shooting, bullet handling, etc.
                spawnTimer += dt;
                if (spawnTimer >= spawnInterval && enemyCount < maxEnemies)
                {
                    spawnTimer = 0.0f;

                    // Spawn new enemy logic
                    Enemy enemy;
                    enemy.position = TileCenter(waypoints[0].row, waypoints[0].col);
                    enemy.speed = enemySpeed;
                    enemy.currentWaypoint = 0;
                    enemy.health = 150.0f;
                    enemy.radius = enemyRadius;
                    enemy.active = true;
                    enemies.push_back(enemy);
                    enemyCount++;
                }

                // Update enemy movement along the path
                for (Enemy& enemy : enemies)
                {
                    if (enemy.active)
                    {
                        Vector2 from = TileCenter(waypoints[enemy.currentWaypoint].row, waypoints[enemy.currentWaypoint].col);
                        Vector2 to = TileCenter(waypoints[enemy.currentWaypoint + 1].row, waypoints[enemy.currentWaypoint + 1].col);
                        Vector2 direction = Normalize(to - from);

                        enemy.position = enemy.position + direction * enemy.speed * dt;

                        if (CheckCollisionPointCircle(enemy.position, to, enemy.radius))
                        {
                            enemy.currentWaypoint++;
                            if (enemy.currentWaypoint >= waypoints.size() - 1)
                            {
                                enemy.active = false; // Reached the end of the path
                            }
                        }
                    }
                }

                // Update turrets, bullets, and other game mechanics
                for (Turret& turret : turrets)
                {
                    turret.reloadTime += dt;
                    Enemy* nearestEnemy = nullptr;
                    float nearestDistance = FLT_MAX;

                    // Find the nearest enemy within range
                    for (Enemy& enemy : enemies)
                    {
                        if (enemy.active)
                        {
                            float distance = Vector2Distance(turret.position, enemy.position);
                            if (distance < nearestDistance && distance <= turret.range)
                            {
                                nearestEnemy = &enemy;
                                nearestDistance = distance;
                            }
                        }
                    }

                    // Fire at the nearest enemy
                    if (nearestEnemy && turret.reloadTime >= turret.fireRate)
                    {
                        turret.reloadTime = 0.0f; // Reset reload timer

                        // Create and fire bullet
                        Bullet bullet;
                        bullet.position = turret.position;
                        bullet.direction = Normalize(nearestEnemy->position - turret.position);
                        bullet.enabled = true;
                        bullets.push_back(bullet);

                        // Apply damage to the enemy
                        nearestEnemy->health -= turret.damage;
                        if (nearestEnemy->health <= 0)
                        {
                            nearestEnemy->active = false; // Kill enemy if health is 0
                        }
                    }
                }

                // Bullet movement and collision detection
                for (Bullet& bullet : bullets)
                {
                    bullet.position = bullet.position + bullet.direction * bulletSpeed * dt;
                    bullet.time += dt;

                    // Check if the bullet collides with an enemy
                    for (Enemy& enemy : enemies)
                    {
                        if (enemy.active && CheckCollisionCircles(bullet.position, bulletRadius, enemy.position, enemy.radius))
                        {
                            // Bullet hits enemy
                            enemy.health -= 10.0f; // Adjust damage as needed
                            bullet.enabled = false; // Disable bullet after impact
                        }
                    }
                }

                // Remove bullets that are no longer active
                bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](Bullet& bullet) {
                    return !bullet.enabled;
                    }), bullets.end());

                // If all enemies are dead, move to the next level
                if (enemies.empty() && enemyCount >= maxEnemies)
                {
                    AdvanceLevel();
                }
            }

            // Rendering
            BeginDrawing();
            ClearBackground(BLACK);

            if (turretCount >= MAX_TURRETS)
            {
                DrawText("Turret limit reached!", 10, 100, 20, RED);
            }


            for (int row = 0; row < TILE_COUNT; row++)
            {
                for (int col = 0; col < TILE_COUNT; col++)
                {
                    DrawTile(row, col, tiles[row][col]);
                }
            }

            // Render turrets, enemies, and bullets
            for (const Turret& turret : turrets)
            {
                DrawCircleV(turret.position, TILE_SIZE * 0.3f, YELLOW); // Draw turret
            }

            for (const Enemy& enemy : enemies)
            {
                if (enemy.active)
                {
                    DrawCircleV(enemy.position, enemy.radius, RED); // Draw enemy
                }
            }

            for (const Bullet& bullet : bullets)
            {
                DrawCircleV(bullet.position, bulletRadius, BLUE); // Draw bullet
            }

            DrawText("Turrets: ", 10, 10, 20, WHITE);
            DrawText(TextFormat("%i", turretCount), 150, 10, 20, WHITE);  // Displays turret count

            DrawText("Level: ", 10, 40, 20, WHITE);
            DrawText(TextFormat("%i", currentLevel), 150, 40, 20, WHITE);  // Displays current level

            DrawText("Enemies: ", 10, 70, 20, WHITE);
            DrawText(TextFormat("%i", enemyCount), 150, 70, 20, WHITE);   // Displays enemy count



            EndDrawing();
        }
        UnloadSound(placeturretSound);  // Unload turret placement sound
        UnloadSound(removeTurretSound); // Unload turret removal sound
        CloseAudioDevice();             // Close the audio device

        CloseWindow();
        return 0;
    }
}