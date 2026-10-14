// Guía de lectura: build del módulo "app".
//  - minSdk 26: Android 8.0 o más nuevo.
//  - compileSdk 37: la plataforma instalada en este equipo.
//  - externalNativeBuild: compila el código C con CMake (ver src/main/cpp/CMakeLists.txt).
//  - abiFilters: solo arm64-v8a (los teléfonos y tabletas reales de hoy); así el APK
//    no carga las bibliotecas de TensorFlow Lite de 4 arquitecturas.
//  - dependencies: TensorFlow Lite "puro" (sin servicios de Google Play, el Huawei no
//    los tiene) para el benchmark del detector de objetos.
//  - OkHttp: cliente WebSocket de la pantalla de control.

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
        versionName = "0.2"

        ndk {
            abiFilters += listOf("arm64-v8a")
        }
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

dependencies {
    implementation("org.tensorflow:tensorflow-lite:2.17.0")
    implementation("org.tensorflow:tensorflow-lite-gpu:2.17.0")
    implementation("org.tensorflow:tensorflow-lite-gpu-api:2.17.0")
    implementation("com.microsoft.onnxruntime:onnxruntime-android:1.20.0")   // SOLO en la rama de investigación (docs/comparativa_detectores.md): el benchmark de modelos .onnx
    implementation("com.squareup.okhttp3:okhttp:4.12.0")   // WebSocket hacia el carro
}
