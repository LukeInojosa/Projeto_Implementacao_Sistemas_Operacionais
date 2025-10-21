# PintOS - Guia de Execução

Este projeto utiliza o sistema operacional educacional **PintOS**. Siga os passos abaixo para rodar o projeto localmente.

## Pré-requisitos

- **Sistema Operacional:** Linux (recomendado Ubuntu)
- **Pacotes necessários:** `build-essential`, `qemu`, `gcc`, `binutils`, `make`, `gdb`

```bash
sudo apt update
sudo apt install build-essential qemu gcc binutils make gdb
```

## Clonando o repositório

```bash
git clone <URL-do-repositório>
cd Projeto_Implementacao_Sistemas_Operacionais
```
## Adicionando PintOS ao Path

Dentro do diretório do projeto, entre na pasta utils e adicione pintos ao path:

```bash
cd utils
echo "PATH=\$PATH:$(pwd)" | sudo tee -a /home/$(whoami)/.bashrc
```
## Compilando o PintOS

No diretório do projeto "/Projeto_Implementacao_Sistemas_Operacionais" execute:

```bash
make clean
make
```

## Rodando o PintOS

Para executar um teste ou o kernel:

```bash
cd threads/build
pintos --qemu --gdb -- run alarm-multiple
```

Substitua `alarm-multiple` pelo teste desejado.

Para rodar os testes:

```bash
cd threads/build
make check
```

## Debugando com GDB

```bash
pintos-gdb kernel.o
```

## Referências

- [Documentação oficial do PintOS](https://web.stanford.edu/class/cs140/projects/pintos/pintos_1.html)
- [Guia de instalação do PintOS](https://github.com/klange/pintos)

---