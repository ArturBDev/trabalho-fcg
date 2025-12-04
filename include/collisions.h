#ifndef COLLISIONS_H
#define COLLISIONS_H

#include <glm/vec4.hpp>
#include <glm/vec4.hpp>
#include <cmath> 

//Fonte: Gemini

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
 * Representa um Cilindro Delimitador (Bounding Cylinder).
 */
struct BoundingCylinder {
    glm::vec4 center; 
    float radius;     
    float height;     
};


/**
 * Calcula a distância euclidiana ao quadrado (evita raiz quadrada).
 */
float distanceSq(const glm::vec4& p1, const glm::vec4& p2);


// --- Funções de Teste de Colisão (Três Tipos Distintos) ---

/**
 * ESFERA-ESFERA (Nave vs. Inimigo)
 */
bool checkSphereSphereCollision(const BoundingSphere& s1, const BoundingSphere& s2);

/**
 * RAIO-ESFERA (Nave (Raio de Trajetória) vs. Checkpoint)
 */
bool checkRaySphereCollision(const Ray& ray, const BoundingSphere& sphere, float& t_out);

/**
 * CILINDRO-ESFERA (Asteroide vs. Nave)
 */
bool checkCylinderSphereCollision(const BoundingCylinder& cylinder, const BoundingSphere& sphere); // << NOVO


float distanceSq(const glm::vec4& p1, const glm::vec4& p2);

extern const float MOON_RADIUS;
extern const float AIRCRAFT_SPHERE_RADIUS;
extern const float CHECKPOINT_RADIUS;
extern const float ASTEROID_CYLINDER_RADIUS;
extern const float ASTEROID_CYLINDER_HEIGHT;


BoundingSphere getEnemyBoundingSphere(const glm::vec4& enemyPosition);
BoundingSphere getAircraftBoundingSphere(const glm::vec4& aircraftPosition);
BoundingSphere getCheckpointBoundingSphere(const glm::vec4& pos);
BoundingCylinder getAsteroidBoundingCylinder(const glm::vec4& pos); 

#endif // COLLISIONS_H