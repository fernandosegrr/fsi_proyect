"""Pruebas de los lectores de versiones sin USB ni teléfono."""
import hashlib
import io
import pathlib
import tempfile
import unittest
import zipfile
import contextlib
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

    @patch.object(v, "consultar_caps")
    @patch.object(v, "coincide_revision", return_value=True)
    @patch.object(v, "ejecutar")
    def test_solo_apk_no_abre_serie(self, ejecutar, revision, caps):
        ejecutar.side_effect = ["package:/data/app/base.apk\n", self.apk("APP:0.4-123abcd"), "versionName=0.4\n", ""]
        salida = io.StringIO()
        with patch("sys.argv", ["verificar_version.py", "--solo-apk"]), contextlib.redirect_stdout(salida):
            self.assertEqual(v.main(), 0)
        caps.assert_not_called()
        self.assertIn("FW PENDIENTE", salida.getvalue())
        revision.assert_called_once_with("123abcd", "HEAD")

    @patch.object(v, "coincide_revision", return_value=False)
    @patch.object(v, "ejecutar")
    def test_solo_apk_rechaza_revision_distinta(self, ejecutar, revision):
        ejecutar.side_effect = ["package:/data/app/base.apk\n", self.apk("APP:0.4-123abcd"), "versionName=0.4\n", ""]
        with patch("sys.argv", ["verificar_version.py", "--solo-apk"]), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(v.main(), 1)

    def _entrega(self, carpeta, nombre, version, suma=None):
        apk = pathlib.Path(carpeta) / nombre
        datos = self.apk(version)
        apk.write_bytes(datos)
        (pathlib.Path(carpeta) / "SHA256SUMS.txt").write_text((suma or hashlib.sha256(datos).hexdigest()) + "  " + nombre + "\n", encoding="utf-8")
        return apk

    @patch.object(v, "consultar_caps")
    @patch.object(v, "coincide_revision", return_value=True)
    @patch.object(v, "ejecutar", return_value="")
    def test_solo_archivo_pasa_sin_telefono(self, ejecutar, revision, caps):
        with tempfile.TemporaryDirectory() as d:
            apk = self._entrega(d, "carro_fsi_0.4-123abcd.apk", "APP:0.4-123abcd")
            salida = io.StringIO()
            with patch("sys.argv", ["verificar_version.py", "--solo-archivo", "--apk", str(apk)]), contextlib.redirect_stdout(salida):
                self.assertEqual(v.main(), 0)
        caps.assert_not_called()
        for llamada in ejecutar.call_args_list:
            self.assertNotIn("adb", " ".join(map(str, llamada.args[0])))   # J16: nunca toca el teléfono
        self.assertIn("PASA", salida.getvalue())
        revision.assert_called_once_with("123abcd", "HEAD")

    @patch.object(v, "coincide_revision", return_value=True)
    @patch.object(v, "ejecutar", return_value="")
    def test_solo_archivo_rechaza_suma_distinta(self, ejecutar, revision):
        with tempfile.TemporaryDirectory() as d:
            apk = self._entrega(d, "carro_fsi_0.4-123abcd.apk", "APP:0.4-123abcd", suma="00" * 32)
            with patch("sys.argv", ["verificar_version.py", "--solo-archivo", "--apk", str(apk)]), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(v.main(), 1)

    @patch.object(v, "coincide_revision", return_value=True)
    @patch.object(v, "ejecutar", return_value="")
    def test_solo_archivo_rechaza_nombre_distinto(self, ejecutar, revision):
        with tempfile.TemporaryDirectory() as d:
            apk = self._entrega(d, "carro_fsi_0.4-9999999.apk", "APP:0.4-123abcd")
            with patch("sys.argv", ["verificar_version.py", "--solo-archivo", "--apk", str(apk)]), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(v.main(), 1)

    @patch.object(v, "coincide_revision", return_value=False)
    @patch.object(v, "ejecutar", return_value="")
    def test_solo_archivo_rechaza_revision_distinta(self, ejecutar, revision):
        with tempfile.TemporaryDirectory() as d:
            apk = self._entrega(d, "carro_fsi_0.4-123abcd.apk", "APP:0.4-123abcd")
            with patch("sys.argv", ["verificar_version.py", "--solo-archivo", "--apk", str(apk)]), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(v.main(), 1)

    def test_solo_archivo_sin_apk_no_pasa(self):
        with patch("sys.argv", ["verificar_version.py", "--solo-archivo"]), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(v.main(), 2)

    def test_solo_apk_y_puerto_son_excluyentes(self):
        with patch("sys.argv", ["verificar_version.py", "--solo-apk", "--puerto", "COM6"]), contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                v.main()


if __name__ == "__main__":
    unittest.main()
