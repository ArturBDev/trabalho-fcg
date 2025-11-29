#ifndef COLLISIONS_H
#define COLLISIONS_H

#include <glm/vec4.hpp>
#include <glm/vec4.hpp>
#include <cmath> 

/**
 * Representa uma Esfera Delimitadora (Bounding Sphere).
 */
struct BoundingSphere {
    glm::vec4 center;
    float radius;
};

/**
 * Representa um Raio (origem e direção).
 */
struct Ray {
    glm::vec4 origin;
    glm::vec4 direction; 
};


/**
 * Calcula a distância euclidiana ao quadrado (evita raiz quadrada).
 */
float distanceSq(const glm::vec4& p1, const glm::vec4& p2);


// --- Funções de Teste de Colisão (Três Tipos Distintos) ---

/**
 * 1. ESFERA-ESFERA (Nave vs. Inimigo)
 */
bool checkSphereSphereCollision(const BoundingSphere& s1, const BoundingSphere& s2);

/**
 * 2. PONTO-ESFERA (Projétil vs. Inimigo)
 */
bool checkPointSphereCollision(const glm::vec4& point, const BoundingSphere& sphere);

/**
 * 3. RAIO-ESFERA (Nave (Raio de Trajetória) vs. Checkpoint)
 */
bool checkRaySphereCollision(const Ray& ray, const BoundingSphere& sphere, float& t_out);

float distanceSq(const glm::vec4& p1, const glm::vec4& p2);

extern const float MOON_RADIUS;
extern const float AIRCRAFT_SPHERE_RADIUS;
extern const float CHECKPOINT_RADIUS;

BoundingSphere getEnemyBoundingSphere(const glm::vec4& enemyPosition);
BoundingSphere getAircraftBoundingSphere(const glm::vec4& aircraftPosition);
BoundingSphere getCheckpointBoundingSphere(const glm::vec4& checkpointPosition);

#endif // COLLISIONS_H