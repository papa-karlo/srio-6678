set C6000_CG_DIR="C:\TI\ccsv6\tools\compiler\c6000_7.4.8"

echo C6000_CG_DIR set as: %C6000_CG_DIR%

echo Converting .out to HEX ...
%C6000_CG_DIR%\bin\hex6x -order L srio_dio_normal.rmd srio_dio_normal.out
..\..\..\..\..\bttbl2hfile\Bttbl2Hfile srio_dio_normal.btbl srio_dio_normal.h srio_dio_normal.bin
copy srio_dio_normal.bin /B ..\..\..\..\..\bin /Y