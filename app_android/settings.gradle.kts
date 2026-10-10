// Guía de lectura: configuración global de Gradle. Dice de dónde bajar los
// plugins y las dependencias (solo repositorios oficiales de Google y Maven)
// y qué módulos tiene el proyecto (uno solo: "app").

pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "CarroTrayectoria"
include(":app")
