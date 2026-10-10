// Guía de lectura: build del módulo "app".
//  - minSdk 26: Android 8.0 o más nuevo.
//  - compileSdk 37: la plataforma instalada en este equipo.
//  - externalNativeBuild: compila el código C con CMake (ver src/main/cpp/CMakeLists.txt).
//  - No hay dependencias externas: solo la Activity clásica de Android.

plugins {
    id("com.android.application")
}

android {
    namespace = "com.fsi.carro"
    compileSdk = 37
    buildToolsVersion = "36.0.0"
    ndkVersion = "30.0.16248370"

    defaultConfig {
        applicationId = "com.fsi.carro"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1"
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "4.1.2"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}
