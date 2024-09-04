#/bin/bash

#Colorcitos!!

greenColour="\e[0;32m\033[1m"
endColour="\033[0m\e[0m"
redColour="\e[0;31m\033[1m"
blueColour="\e[0;34m\033[1m"
yellowColour="\e[0;33m\033[1m"
purpleColour="\e[0;35m\033[1m"
turquoiseColour="\e[0;36m\033[1m"
grayColour="\e[0;37m\033[1m" 


#compilo librearia global

echo -e "\n${greenColour}[*]${endColour}${grayColour}Compilando Libreria global ...${endColour}\n\n"


cd ../global
make clean all

target=$(find . -name libglobal.a)
if [ "$target" != "./bin/libglobal.a" ]; then
	echo -e "\n${redColour}[*]${endColour}${grayColour}Error de compilacion. Saliendo...${endColour}\n\n"
	exit 0
fi

#compilo los modulos

echo -e "\n${greenColour}[*]${endColour}${grayColour}Compilando Memoria ...${endColour}\n\n"


cd ../memoria 
make clean all

target=$(find . -name memoria.out)
if [ "$target" != "./bin/memoria.out" ]; then
	echo -e "\n${redColour}[*]${endColour}${grayColour}Error de compilacion. Saliendo...${endColour}\n\n"
	exit 0
fi

echo -e "\n${greenColour}[*]${endColour}${grayColour}Compilando CPU ...${endColour}\n\n"


cd ../CPU
make clean all

target=$(find . -name CPU.out)
if [ "$target" != "./bin/CPU.out" ]; then
	echo -e "\n${redColour}[*]${endColour}${grayColour}Error de compilacion. Saliendo...${endColour}\n\n"
	exit 0
fi

echo -e "\n${greenColour}[*]${endColour}${grayColour}Compilando Filesystem ...${endColour}\n\n"


cd ../filesystem
make clean all

target=$(find . -name filesystem.out)
if [ "$target" != "./bin/filesystem.out" ]; then
	echo -e "\n${redColour}[*]${endColour}${grayColour}Error de compilacion. Saliendo...${endColour}\n\n"
	exit 0
fi

echo -e "\n${greenColour}[*]${endColour}${grayColour}Compilando Kernel ...${endColour}\n\n"

cd ../kernel
make clean all

target=$(find . -name kernel.out)
if [ "$target" != "./bin/kernel.out" ]; then
	echo -e "\n${redColour}[*]${endColour}${grayColour}Error de compilacion. Saliendo...${endColour}\n\n"
	exit 0
fi

