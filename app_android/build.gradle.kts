// Guía de lectura: build raíz. Solo declara la versión del plugin de Android
// (AGP); el módulo "app" lo aplica en app/build.gradle.kts.
// Con AGP 9 el soporte de Kotlin ya viene incluido, no hace falta otro plugin.

plugins {
    id("com.android.application") version "9.4.1" apply false
}
