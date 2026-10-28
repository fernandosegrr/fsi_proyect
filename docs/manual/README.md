# Construcción del manual

El manual se redacta en `capitulos/`. El generador usa ReportLab para la composición y el índice multipasada, matplotlib para los diagramas y PyMuPDF para limpiar recursos tipográficos sin uso. Esta combinación conserva el entorno local del proyecto y permite verificar el resultado sin instalar herramientas globales.

Con las dependencias de `requisitos.txt` instaladas en el entorno virtual del repositorio, ejecutar desde la raíz:

```powershell
.\.venv\Scripts\python.exe docs\manual\generar_manual.py
```

El comando reconstruye las figuras y el PDF. La portada identifica el commit de fuentes observado al construir; el PDF agregado en un commit posterior no puede contener el hash de ese mismo commit sin una autorreferencia circular. Las fuentes que se encuentren sin confirmar no quedan identificadas por ese hash.
