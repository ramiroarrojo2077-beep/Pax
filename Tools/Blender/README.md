# Generador de assets

```bash
blender --background --python Tools/Blender/build_assets.py -- --all
```

Genera en `Tools/Blender/Build/` el monoplaza (malla esqueletal con sus seis
huesos), el circuito y el CSV del eje de pista que importa `ATrackSpline`.

Las cotas del coche y los puntos del trazado están en `pax_blender/config.py`:
cambiar un número y volver a ejecutar es todo el flujo de trabajo.

La documentación completa —convenio de ejes, escala, esqueleto y por qué el
flap del DRS va en un objeto aparte— está en
[`Docs/BLENDER_PIPELINE.md`](../../Docs/BLENDER_PIPELINE.md).
