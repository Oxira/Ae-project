# ============================================================
#  Brigade des Sangliers - Dockerfile Backend Java
#  Build multi-stage : compile + image minimale JRE
# ============================================================

# --- Stage 1 : compilation Maven ---
FROM maven:3.9-eclipse-temurin-17 AS build
WORKDIR /app

# Copier le pom.xml en premier pour profiter du cache des couches
COPY pom.xml .
RUN mvn dependency:go-offline -q

# Copier les sources
COPY src ./src

# Copier le frontend dans static/ avant le build
# (le dossier web/ de la racine du repo est monte via docker-compose)
COPY ../web ./src/main/resources/static

RUN mvn package -DskipTests -q

# --- Stage 2 : image runtime minimale ---
FROM eclipse-temurin:17-jre-alpine
WORKDIR /app

# Repertoire de donnees persistent (historique JSON)
RUN mkdir -p /app/data
VOLUME ["/app/data"]

COPY --from=build /app/target/brigade-backend-*.jar app.jar

EXPOSE 8080

ENTRYPOINT ["java", "-jar", "app.jar"]
