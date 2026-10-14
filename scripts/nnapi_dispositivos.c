/*
 * nnapi_dispositivos.c — Lista los dispositivos que Android ofrece a NNAPI (CPU de referencia, GPU, aceleradores como una NPU), solo nombres y tipos.
 *
 * GUÍA DE LECTURA
 *  1. Sirve para la comparativa de detectores (docs/comparativa_detectores.md): saber si el teléfono expone una NPU a NNAPI (tipo ACCELERATOR) o solo la CPU de referencia.
 *     Es un HECHO que se puede comprobar; que un modelo corra de verdad en esa NPU se prueba aparte (tiempos con el delegado fijado a ese dispositivo).
 *  2. Se compila con el NDK para el teléfono (API 29 o mayor; el P40 Pro tiene Android 10 = API 29) y se corre con adb shell. Ver scripts/medir_detectores.py.
 *  3. Escribe una línea por dispositivo:  NNAPI_DISPOSITIVO indice=0 nombre=... tipo=CPU|GPU|ACCELERATOR|OTHER|UNKNOWN nivel=... version=...
 */
#include <android/NeuralNetworks.h>
#include <stdio.h>

int main(void)
{
    uint32_t n = 0;
    uint32_t i;

    if (ANeuralNetworks_getDeviceCount(&n) != ANEURALNETWORKS_NO_ERROR) {
        printf("NNAPI_ERROR no se pudo contar los dispositivos\n");
        return 1;
    }
    printf("NNAPI_DISPOSITIVOS total=%u\n", (unsigned)n);
    for (i = 0; i < n; i++) {
        ANeuralNetworksDevice *d = NULL;
        const char *nombre = "?", *version = "?";
        int32_t tipo = 0;
        int64_t nivel = 0;
        const char *texto_tipo = "UNKNOWN";

        if (ANeuralNetworks_getDevice(i, &d) != ANEURALNETWORKS_NO_ERROR || d == NULL) {
            printf("NNAPI_DISPOSITIVO indice=%u error=no se pudo abrir\n", (unsigned)i);
            continue;
        }
        ANeuralNetworksDevice_getName(d, &nombre);
        ANeuralNetworksDevice_getVersion(d, &version);
        ANeuralNetworksDevice_getType(d, &tipo);
        ANeuralNetworksDevice_getFeatureLevel(d, &nivel);
        switch (tipo) {
        case ANEURALNETWORKS_DEVICE_CPU:         texto_tipo = "CPU"; break;
        case ANEURALNETWORKS_DEVICE_GPU:         texto_tipo = "GPU"; break;
        case ANEURALNETWORKS_DEVICE_ACCELERATOR: texto_tipo = "ACCELERATOR"; break;
        case ANEURALNETWORKS_DEVICE_OTHER:       texto_tipo = "OTHER"; break;
        default:                                 texto_tipo = "UNKNOWN"; break;
        }
        printf("NNAPI_DISPOSITIVO indice=%u nombre=%s tipo=%s nivel=%lld version=%s\n", (unsigned)i, nombre, texto_tipo, (long long)nivel, version);
    }
    return 0;
}
