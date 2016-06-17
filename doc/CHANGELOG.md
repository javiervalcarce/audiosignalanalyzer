Changelog
=========

All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).

See also [how to write a changelog](http://keepachangelog.com/).

----------------------------------------------------------------------------------------------------
## 2016.06.17 -> 0.5.0
### Changes
- ?
###Bugs
- No

----------------------------------------------------------------------------------------------------
## 2016.04.21 -> 0.4.2
### Bugs
- Output help text test_thd_analyzer --help was wrong/incomplete

----------------------------------------------------------------------------------------------------
## 2016.03.08 -> 0.4.1
### Bugs
- Analyzer thread freezes if Start() is not called.

----------------------------------------------------------------------------------------------------
## 2016.03.02 -> 0.4.0
### Changes
- Añado thd_analizer::VersionString(), thd_analyzer::BuildDate(), etc. Para poder consultar
  por programa la versión del componente IP.
### Bugs
- En el destructor de ThdAnalyzer se espera por el hilo solamente si este ha sido
  creado con éxito, de lo contrario esperaríamos para siempre.

----------------------------------------------------------------------------------------------------
## 2016.02.10 -> 0.3.0
### Changes
- Añado ThdAnalyzer::Start() y ThdAnalyzer::Stop() para poner en marcha/parar el analizador.
- Compara el espectro con una máscara f(x) configurable y anota el número de frecuencias que
  traspasan la máscara.
- La frecuencia de muestreo (Fs) es configurable, si la especificada no existe en el
  hardware se elige la inmediatamente superior disponible. Solo se trabaja con Fs nativas del
  hardware evitando se evita el cambio en la velocidad de muestreo por software.
- Las muestras ahora son de 32 bits, si el ADC tuviese menos bits entonces los de menor
  peso se dejan a cero.

----------------------------------------------------------------------------------------------------
## 2016.01.22 -> 0.2.0
### Changes
- Cálculo de la SNRI (Relación Señal a Ruido + Interferencias)
- Ahorro de memoria
- Permite volcar la gráfica del espectro a un fichero de texto para visualizarlo con octave.

----------------------------------------------------------------------------------------------------
## 2016.01.07 -> 0.1.1
### Changes
- Arreglo un bug consistente en que no se destruia adecuadamente el hilo de procesado, causando, en
  ocasiones, SIGSEGV en la aplicación.

----------------------------------------------------------------------------------------------------
## 2015.12.04 -> 0.1.0
### Changes
- Primera versión del módulo.
- Este componente es una librería dinámica que permite capturar y analizar señales de audio.
----------------------------------------------------------------------------------------------------


