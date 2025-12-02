#include "collisions.h"

#include <glm/vec4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/common.hpp> 
#include <algorithm>
#include <limits>
#include <cmath> 

const float MOON_RADIUS = 16.0f;
const float AIRCRAFT_SPHERE_RADIUS = 0.3f; 
const float CHECKPOINT_RADIUS = 1.5f; 
const float ASTEROID_CYLINDER_RADIUS = 0.5f; 
const float ASTEROID_CYLINDER_HEIGHT = 1.0f; 


/**
 * Calcula a distância euclidiana ao quadrado (evita raiz quadrada).
 */
float distanceSq(const glm::vec4& p1, const glm::vec4& p2) {
    glm::vec4 diff = p1 - p2;
    diff.w = 0.0f; 

    //não conseguimos usar o dotproduct direto por conta do conflito de includes
    return dot(diff, diff); 
}

/**
 * Teste de Intersecção ESFERA-ESFERA (Nave vs. Inimigo)
 */
bool checkSphereSphereCollision(const BoundingSphere& s1, const BoundingSphere& s2) {
    float distanceSquared = distanceSq(s1.center, s2.center); 
    
    float radiusSum = s1.radius + s2.radius;
    float radiusSumSquared = radiusSum * radiusSum;

    return distanceSquared <= radiusSumSquared;
}

/**
 * Teste de Intersecção RAIO-ESFERA (Nave (Raio de Trajetória) vs. Checkpoint)
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


/**
 * Teste de Intersecção CILINDRO-ESFERA (Asteroide vs. Nave)
 */
bool checkCylinderSphereCollision(const BoundingCylinder& cylinder, const BoundingSphere& sphere) {
    // 1. Checagem Horizontal/Radial (em 2D - plano XZ)
    
    // Projeta o centro da esfera no plano XZ do centro do cilindro
    glm::vec4 sphereCenterXZ = sphere.center;
    sphereCenterXZ.y = cylinder.center.y;
    
    // Distância horizontal ao quadrado
    float distanceXZSquared = distanceSq(cylinder.center, sphereCenterXZ);

    float radialRadiusSum = cylinder.radius + sphere.radius;
    if (distanceXZSquared > (radialRadiusSum * radialRadiusSum)) {
        return false; // Não colide radialmente
    }
    
    // 2. Checagem Vertical (Eixo Y)
    
    float halfHeight = cylinder.height / 2.0f;
    
    float cylinderMinY = cylinder.center.y - halfHeight;
    float cylinderMaxY = cylinder.center.y + halfHeight;
    
    float sphereMinY = sphere.center.y - sphere.radius;
    float sphereMaxY = sphere.center.y + sphere.radius;

    // Checa sobreposição dos intervalos [MinY, MaxY]
    if (sphereMaxY < cylinderMinY || sphereMinY > cylinderMaxY) {
        return false; // Não colide verticalmente
    }

    // Se houver sobreposição em ambos os eixos, há colisão.
    return true;
}


BoundingCylinder getAsteroidBoundingCylinder(const glm::vec4& asteroidPosition) {
    return {
        asteroidPosition,
        ASTEROID_CYLINDER_RADIUS,
        ASTEROID_CYLINDER_HEIGHT
    };
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