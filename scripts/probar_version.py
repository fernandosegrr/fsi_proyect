"""Pruebas de los lectores de versiones sin USB ni teléfono."""
import io
import unittest
import zipfile
from unittest.mock import patch
import verificar_version as v


class Versiones(unittest.TestCase):
    def apk(self, texto):
        salida = io.BytesIO()
        with zipfile.ZipFile(salida, "w") as z:
            z.writestr("assets/version_app.txt", texto)
        return salida.getvalue()

    def test_fw(self):
        self.assertEqual(v.version_fw("CAPS:IMU=1,FW:123abcd-20261020"), ("FW:123abcd-20261020", "123abcd"))

    def test_fw_viejo(self):
        with self.assertRaises(ValueError):
            v.version_fw("CAPS:IMU=1,TOF=1")

    def test_apk(self):
        self.assertEqual(v.version_apk(self.apk("APP:0.4-123abcd\n")), ("APP:0.4-123abcd", "123abcd"))

    def test_apk_invalida(self):
        with self.assertRaises(ValueError):
            v.version_apk(self.apk("APP:0.4-sin-version"))

    @patch.object(v, "ejecutar", side_effect=["abc", "abc"])
    def test_misma_revision(self, ejecutar):
        self.assertTrue(v.coincide_revision("abc", "HEAD"))

    @patch.object(v.subprocess, "run")
    @patch.object(v, "ejecutar", side_effect=["abc", "def", "docs/reporte_J13.md\n"])
    def test_solo_documentacion(self, ejecutar, proceso):
        proceso.return_value.returncode = 0
        self.assertTrue(v.coincide_revision("abc", "HEAD"))

    @patch.object(v.subprocess, "run")
    @patch.object(v, "ejecutar", side_effect=["abc", "def", "docs/reporte_J14.md\ndist/SHA256SUMS.txt\n"])
    def test_solo_documentacion_y_entrega(self, ejecutar, proceso):
        proceso.return_value.returncode = 0
        self.assertTrue(v.coincide_revision("abc", "HEAD"))

    @patch.object(v.subprocess, "run")
    @patch.object(v, "ejecutar", side_effect=["abc", "def", "README.md\nscripts/verificar_version.py\nscripts/probar_version.py\n"])
    def test_readme_y_verificador(self, ejecutar, proceso):
        proceso.return_value.returncode = 0
        self.assertTrue(v.coincide_revision("abc", "HEAD"))

    @patch.object(v.subprocess, "run")
    @patch.object(v, "ejecutar", side_effect=["abc", "def", "scripts/cargar.ps1\n"])
    def test_otro_script(self, ejecutar, proceso):
        proceso.return_value.returncode = 0
        self.assertFalse(v.coincide_revision("abc", "HEAD"))

    @patch.object(v.subprocess, "run")
    @patch.object(v, "ejecutar", side_effect=["abc", "def", "firmware_carro/imu.c\n"])
    def test_cambio_codigo(self, ejecutar, proceso):
        proceso.return_value.returncode = 0
        self.assertFalse(v.coincide_revision("abc", "HEAD"))


if __name__ == "__main__":
    unittest.main()
