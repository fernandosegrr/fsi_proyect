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

val hashGit = providers.exec {
    workingDir(rootProject.projectDir.parentFile)
    commandLine("git", "rev-parse", "--short=7", "HEAD")
}.standardOutput.asText.get().trim()
val nombreVersion = "0.4"
val generarVersion = tasks.register("generarVersion") {
    inputs.property("hashGit", hashGit)
    inputs.property("versionName", nombreVersion)
    val destino = layout.buildDirectory.dir("generated/versionAssets")
    outputs.dir(destino)
    doLast {
        val archivo = destino.get().file("version_app.txt").asFile
        archivo.parentFile.mkdirs()
        archivo.writeText("APP:$nombreVersion-$hashGit\n")
    }
}
tasks.matching { it.name == "preBuild" }.configureEach { dependsOn(generarVersion) }

android {
    namespace = "com.fsi.carro"
    compileSdk = 37
    buildToolsVersion = "36.0.0"
    ndkVersion = "30.0.16248370"

    defaultConfig {
        applicationId = "com.fsi.carro"
        minSdk = 26
        targetSdk = 35
        versionCode = 3
        versionName = nombreVersion
        buildConfigField("String", "GIT_HASH", "\"$hashGit\"")
        // Prueba de instrumentación para las pruebas físicas J14: pulsa botones por resource-id
        // (sin coordenadas fijas ni capturas de la pantalla con cámara).
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        ndk {
            abiFilters += listOf("arm64-v8a")
        }
    }

    buildFeatures { buildConfig = true }
    sourceSets["main"].assets.directories.add(layout.buildDirectory.dir("generated/versionAssets").get().asFile.absolutePath)

    // J14: comprimir las bibliotecas nativas dentro del APK. Sin esto el APK de entrega supera
    // los 15 MB; Android las extrae al instalar y System.loadLibrary las sigue encontrando.
    packaging {
        jniLibs {
            useLegacyPackaging = true
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
    implementation("com.squareup.okhttp3:okhttp:4.12.0")   // WebSocket hacia el carro
    // Capa visual (Views, sin Compose): componentes Material 3, AppCompat (tema claro/oscuro), Fragment (una sola pantalla con barra inferior) y ConstraintLayout.
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("androidx.fragment:fragment:1.8.5")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    // Solo para las pruebas físicas por ADB (src/androidTest): pulsar por resource-id.
    androidTestImplementation("androidx.test:runner:1.7.0")
    androidTestImplementation("androidx.test.ext:junit:1.3.0")
    androidTestImplementation("androidx.test.uiautomator:uiautomator:2.3.0")
}
