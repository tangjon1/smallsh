# SmallSh
## Notes
Portfolio Project for Operating Systems course.<br />
The ```str_gsub.c``` code is adapted from provided course material.<br />
## Description
SmallSh is a simple shell program in C. After receiving user input, it is capable of expanding some parameters (```~/```, ```$$```, ```$?```, ```$!```) and parsing certain operators (```#```, ```&```, ```<```, ```>```). SmallSh can directly run two built-in commands, ```exit``` and ```cd```. Otherwise, arguments are passed to an ```execvp``` function to be run either in the foreground or the background in a new process.<br />
## Instructions
Use the ```makefile``` from the terminal and run the created ```smallsh``` executable. 