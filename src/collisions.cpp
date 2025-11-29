#include "collisions.h"

#include <glm/vec4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/common.hpp> 
#include <algorithm>
#include <limits>
#include <cmath> 

const float MOON_RADIUS = 16.0f;
const float AIRCRAFT_SPHERE_RADIUS = 0.5f; 
const float CHECKPOINT_RADIUS = 2.5f; 


/**
 * Calcula a distância euclidiana ao quadrado (evita raiz quadrada).
 */
float distanceSq(const glm::vec4& p1, const glm::vec4& p2) {
    glm::vec4 diff = p1 - p2;
    // O produto escalar (dot product) de um vetor por ele mesmo é a norma ao quadrado.
    return glm::dot(diff, diff); 
}

/**
 * 1. Teste de Intersecção ESFERA-ESFERA (Nave vs. Inimigo)
 */
bool checkSphereSphereCollision(const BoundingSphere& s1, const BoundingSphere& s2) {
    float distanceSquared = distanceSq(s1.center, s2.center); 
    
    float radiusSum = s1.radius + s2.radius;
    float radiusSumSquared = radiusSum * radiusSum;

    return distanceSquared <= radiusSumSquared;
}

/**
 * 2. Teste de Intersecção PONTO-ESFERA (Projétil vs. Inimigo)
 */
bool checkPointSphereCollision(const glm::vec4& point, const BoundingSphere& sphere) {
    float distanceSquared = distanceSq(point, sphere.center); 
    
    float radiusSquared = sphere.radius * sphere.radius;

    return distanceSquared <= radiusSquared;
}

/**
 * 3. Teste de Intersecção RAIO-ESFERA (Nave (Raio de Trajetória) vs. Checkpoint)
 */
bool checkRaySphereCollision(const Ray& ray, const BoundingSphere& sphere, float& t_out) {
    // Vetor do centro do raio (c) para o centro da esfera (s): L = c - s
    glm::vec4 L = ray.origin - sphere.center;

    // A = ||d||^2. Assume-se ray.direction é normalizado, A = 1.0f.
    float A = 1.0f; 

    // B = 2 * d . L
    float B = 2.0f * glm::dot(ray.direction, L);

    // C = ||L||^2 - r^2. Usando glm::dot para a norma ao quadrado.
    float C = glm::dot(L, L) - (sphere.radius * sphere.radius);

    // Calcula o discriminante (Delta)
    float discriminant = B * B - 4.0f * A * C;

    if (discriminant < 0.0f) {
        return false;
    }

    // Resolve a equação quadrática
    float sqrtDiscriminant = std::sqrt(discriminant);
    float t1 = (-B - sqrtDiscriminant) / (2.0f * A);
    float t2 = (-B + sqrtDiscriminant) / (2.0f * A);

    // Pega o t mais próximo e positivo
    t_out = std::numeric_limits<float>::max();

    if (t1 >= 0.0f) {
        t_out = t1;
    }
    
    if (t2 >= 0.0f && t2 < t_out) {
        t_out = t2;
    }

    if (t_out == std::numeric_limits<float>::max()) {
        return false;
    }

    return true;
}


BoundingSphere getEnemyBoundingSphere(const glm::vec4& enemyPosition) {
    return {
        glm::vec4(enemyPosition),
        AIRCRAFT_SPHERE_RADIUS
    };
}

BoundingSphere getAircraftBoundingSphere(const glm::vec4& aircraftPosition) {
    return {
        glm::vec4(aircraftPosition),
        AIRCRAFT_SPHERE_RADIUS
    };
}

BoundingSphere getCheckpointBoundingSphere(const glm::vec4& checkpointPosition) {
    return {
        checkpointPosition,
        CHECKPOINT_RADIUS
    };
}