"""Runner de pruebas para PlatformIO.

Las pruebas de este proyecto son programas `main()` normales, no suites de
Unity: asi el mismo fichero se compila y se ejecuta con `g++` a secas, sin
depender de PlatformIO, que es como se verifican en un equipo sin toolchain.

Este runner traduce su salida al formato que espera `pio test`, para que
`pio test -e host` de un resultado por comprobacion en vez de un unico
aprobado/suspenso por binario.

Convenio de la salida (ver test/test_nucleo/main.cpp):
    "  ok     <nombre>"   comprobacion superada
    "  FALLO  <nombre>"   comprobacion fallida
"""

from platformio.public import TestCase, TestCaseSource, TestRunnerBase, TestStatus


class CustomTestRunner(TestRunnerBase):
    def on_testing_line_output(self, line):
        if self.options.verbose:
            print(line, end="")

        texto = line.strip()
        if not texto:
            return

        if texto.startswith("ok "):
            estado, nombre = TestStatus.PASSED, texto[3:].strip()
        elif texto.startswith("FALLO "):
            estado, nombre = TestStatus.FAILED, texto[6:].strip()
        else:
            # Lineas de contexto (encabezados de bloque, cifras medidas).
            return

        self.test_suite.add_case(
            TestCase(
                name=nombre,
                status=estado,
                message=None if estado == TestStatus.PASSED else texto,
                source=TestCaseSource(filename=self.test_suite.test_name),
            )
        )

    def teardown(self):
        # Un binario que no emitio ninguna comprobacion es un fallo: significa
        # que reventó antes de empezar, no que no hubiera nada que probar.
        if not self.test_suite.cases:
            self.test_suite.add_case(
                TestCase(
                    name="el binario no produjo ninguna comprobacion",
                    status=TestStatus.FAILED,
                    source=TestCaseSource(filename=self.test_suite.test_name),
                )
            )
