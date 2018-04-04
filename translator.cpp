#include <iostream>
#include <cstdio>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ugpio/ugpio.h>
#include <fstream>

using namespace std;

//Classes, structs, enums, constants, and global variables

//Class that handles all of the file writing
class FileWriter
{
	//private variables
	private:
		ofstream out;
		int severity;
		clock_t time;
	//public functions
	public:
		//constructor opens the file for writing
		FileWriter(char name[])
		{
			out.open(name);
			if (!out.is_open())
				cerr<<"ERROR: can't open output file";
			time = clock();
		}
		//a setter for the severity variable in case mid program we want to reduce/increase the logging
		void setSeverity(int num)
		{
			severity=num;
			return;
		}
		//The write function handles both priority and writing to the file; 
		void write(char* message, int num)
		{
			if(num<=severity)
				out<<(clock()-time)<<":\t"<<message<<endl;
			return;
		}
		//destructor closes the file
		~FileWriter()
		{
			out.close();
		}
};



//Struct to store a "pair" or code
struct codepair
{
	char letter;			//english character
	int size_of_code;		//length of the morse code
	char code[8];			//the morse code, not to be confused with CODE which is an enum
};

enum State {sending, receiving, done};	//enum to store the state of the user's code
enum CODE {dot, dash, eoc, err};	//enum to store the morse code character, and error just in case

#define SCALE 200000			//constant that defines how long a character takes to signal

codepair characters[38];		//global variable that stores the english alphabet and its morse code equivalents
FileWriter fw("log.txt");		//global variable that stores the filewriter that our code uses
int MAXCHAR = 38;



//FUNCTIONS


//high-level functions
State cont();							//returns the state of the code, either sending, receiving, or done
void init();							//initializes the character array
void recieveRoutine();						//the routine that the code goes through when the user is receiving the message
void sendRoutine();						//the routine that the code goes through when the user is sending the message

//input helper functions
bool startOutput();						//waits for a signal for the other omega to start sending the message (might not be implemented)
CODE inputCheck(int pin);					//returns the type of morse code character that is being sent either dot, dash, or eoc (end of character)
void validInput(char input[]);					//returns valid input 
void translateCode(char input[], char result[501][8]);		//translates the valid input into morsecode
void outputCode(char result[501][8]);				//outputs the morsecode

//output helper functions
void outputPrep(int pin);					//sends a signal to let the other omega know that it is ready
void outputDash(int pin);					//outputs a dash
void outputDot(int pin);					//outputs a dot
void outputEOC(int pin);					//outputs a signal that indicates the end of a character
void freeOutputPin(int pin);					//frees the output pin


//Defining the functions:

State cont()
{
	//Ask the user for input
	char input[10];
	State ans;
	cout<<"Are you sending or receiving? Enter something asides from \"sending\" and \"receiving\" to quit: ";
	fgets(input, 9, stdin);
	
	//Assign the type of state that was inputted into ans, then return it. 
	if(!(strcmp("sending", input)) || !(strcmp("sending\n", input)))
	{
		ans = sending;
	}
	
	else if(!(strcmp("receiving",input)) || !(strcmp("sending\n", input)))
	{
		ans = receiving;
	}
	
	else
	{
		ans = done;
	}
	
	return ans;
}

void validInput(char input[])
{
	//I'm not comfortable with do-while loops, so I used this approach. I do recognize that for this purpose a do while is more suitable	
	bool valid = false;
	//as long as the input isn't valid then keep getting the user to input another string.
	while(!valid)
	{
		cout<<"Please enter your message: ";
		fgets(input, 500, stdin);
		int index=0;
		valid=true;
		//if a letter is lowercase, it will be changed to its uppercase equivalent.
		//however, if it isn't a number or a letter, (or a space) then it is an invalid string.
		while(input[index]!=0 && input[index]!='\n')
		{
			if(input[index]>='a' && input[index]<='z')
				{
					input[index]-=('a'-'A');
				}
			else if(!(input[index]>='A' && input[index]<='Z') && !(input[index]>='0' && input[index]<='9') && input[index]!=' ')
			{
				valid=false;
				break;
			}
			index++;
		}
		//If the input wasn't valid kindly remind the user that we are humans who don't have time to implement every single character as some form of morse code (We didn't think about using ascii)
		if(!valid)
			cout<<"Please enter a valid input (Only uppercase characters, numbers, and spaces allowed)"<<endl;
	}
}
bool startOutput()
{
	int rq, rv;
	int gpio;
	int value;
	gpio = 3;
	int tmp=0;
	// check if gpio is already exported
	if ((rq = gpio_is_requested(gpio)) < 0)
	{
		cerr<<("gpio_is_requested");
		return err;
	}
	// export the gpio
	if (!rq) {
		cout<<("> exporting gpio\n");
		if ((rv = gpio_request(gpio, NULL)) < 0)
		{
			cerr<<("gpio_request");
			return err;
		}
	}
	
	// set to input direction
	cout<<("> setting to input\n");
	if ((rv = gpio_direction_input(gpio)) < 0)
	{
		cerr<<("gpio_direction_input");
	}
	//as long as there is a current going  through the pin, the signal to start hasn't been sent. 
	value=1;
	while(value)
	{
		//cout<<"waiting on val"<<tmp++<<endl;
		value = gpio_get_value(gpio);
		if(!value)
		{
			//once the signal is sent, then sleep till the light turns off and return true since the signal was found.
			if (!rq) {
				cout<<("> unexporting gpio\n");
				if (gpio_free(gpio) < 0)
				{
					cerr<<("gpio_free");
				}
			}			
			usleep(SCALE*0.5);
			return true;
		}
	}

	

	// unexport the gpio
	if (!rq) {
		cout<<("> unexporting gpio\n");
		if (gpio_free(gpio) < 0)
		{
			cerr<<("gpio_free");
		}
	}
	return false;

}
CODE inputCheck(int pin)
{
	int dot_time = (int)(SCALE*0.25);
	int i;
	int rq, rv;
	int gpio;
	int value;
	CODE ans=err;
	gpio = 3;
	int tmp=0;
	// check if gpio is already exported
	if ((rq = gpio_is_requested(gpio)) < 0)
	{
		cerr<<("gpio_is_requested");
		return err;
	}
	// export the gpio
	if (!rq) {
		cout<<("> exporting gpio\n");
		if ((rv = gpio_request(gpio, NULL)) < 0)
		{
			cerr<<("gpio_request");
			return err;
		}
	}
	
	// set to input direction
	cout<<("> setting to input\n");
	if ((rv = gpio_direction_input(gpio)) < 0)
	{
		cerr<<("gpio_direction_input");
	}
	
	cout<<"Begin reading\n";
	value=1;
	//As long as there is an input, then keep the loop going
	while(value)
	{
		//cout<<"waiting on val"<<tmp++<<endl;
		value = gpio_get_value(gpio);
		if(!value)
		{
			//if there is a signal, then wait for the length of a dot
			usleep(dot_time+101);
			value = gpio_get_value(gpio);
			if(!value)
			{
				//if the signal is still there then it's either a dot or a end of character. Wait for another dot's duration + a bit
				usleep(dot_time+101);
				value = gpio_get_value(gpio);
				if(!value)
				{
					//if the signal is still there it must be a end of character, set ans to eoc, then wait the rest of the time out and break
					ans=eoc;
					usleep(SCALE-2*(dot_time+101));   //used to be -2*50101
					break;
				}
				//if the signal is no longer there, then it must be a dash. Follow the same jig as w/ eoc
				ans= dash;
				usleep(SCALE-2*(dot_time+101));
				break;
			}
			//if after sleeping for a bit longer than a dot the signal is gone, it must be a dot. follow the same procedure as above
			else
			{
				usleep(SCALE-2*(dot_time+101));  //should be 3, but it doesn't hurt to be safe
				ans= dot;
				break;
			}
		}
	}

	

	// unexport the gpio
	if (!rq) {
		cout<<("> unexporting gpio\n");
		if (gpio_free(gpio) < 0)
		{
			cerr<<("gpio_free");
		}
	}
	//return the findings
	return ans;
}

//Function that outputs a dash with a rest to make the time per character uniform
void outputDash(int pin)
{
	gpio_direction_output(pin,1);
	usleep(0.5*SCALE);
	gpio_direction_output(pin,0);
	usleep(0.5*SCALE);
	return;
}

//Function that outputs a dot with a rest to make the time per character uniform
void outputDot(int pin)
{
	gpio_direction_output(pin,1);
	usleep(0.25*SCALE);
	gpio_direction_output(pin,0);
	usleep(0.75*SCALE);
	return;
}

//Function that outputs an end of character with a rest to make the time per character uniform
void outputEOC(int pin)
{
	gpio_direction_output(pin,1);
	usleep(0.75*SCALE);
	gpio_direction_output(pin,0);
	usleep(0.25*SCALE);
	return;
}

//Function that signals the other onion that it is ready to read input (might not be implemented)
void outputPrep(int pin)
{
	gpio_direction_output(pin,1);
	usleep(SCALE);
	gpio_direction_output(pin,0);
	return;
}

//Function that initializes the output pin
void initOutputPin(int pin)
{
	gpio_request(pin, NULL);
	return;
}

//Function that frees an output pin
void freeOutputPin(int pin)
{
	gpio_free(pin);
	return;
}

//Function that initializes all of the data required in this program asides from the user input
void init()
{
	//set the severity of the logging
	fw.setSeverity(0);
	fw.write("START", 0);
	//dictionary of characters
	fw.write("init code pair array", 10);	

	

	//Array to store the morse code translation of each letter in the input.
	
	
	/////////////////////////////////////FORMATTING MORSE CODE DICTIONARY/////////////////////////////////////////

	////////////////////////////////////////////////LETTERS///////////////////////////////////////////////////////
	strcpy(characters[0].code 	,".-"	);	characters[0].letter='A' ;	characters[0].size_of_code=2;
	strcpy(characters[1].code 	,"-..."	);	characters[1].letter='B' ;	characters[1].size_of_code=4;
	strcpy(characters[2].code 	,"-.-."	);	characters[2].letter='C' ;	characters[2].size_of_code=4;
	strcpy(characters[3].code 	,"-.."	);	characters[3].letter='D' ;	characters[3].size_of_code=3;
	strcpy(characters[4].code 	,"."	);	characters[4].letter='E' ;	characters[4].size_of_code=1;
	strcpy(characters[5].code 	,"..-."	);	characters[5].letter='F' ;	characters[5].size_of_code=4;
	strcpy(characters[6].code 	,"--."	);	characters[6].letter='G' ;	characters[6].size_of_code=3;
	strcpy(characters[7].code 	,"...."	);	characters[7].letter='H' ;	characters[7].size_of_code=4;
	strcpy(characters[8].code 	,".."	);	characters[8].letter='I' ;	characters[8].size_of_code=2;
	strcpy(characters[9].code	,".---"	);	characters[9].letter='J' ;	characters[9].size_of_code=4;
	strcpy(characters[10].code	,"-.-"	);	characters[10].letter='K';	characters[10].size_of_code=3;
	strcpy(characters[11].code	,".-.."	);	characters[11].letter='L';	characters[11].size_of_code=4;
	strcpy(characters[12].code	,"--"	);	characters[12].letter='M';	characters[12].size_of_code=2;
	strcpy(characters[13].code	,"-."	);	characters[13].letter='N';	characters[13].size_of_code=2;
	strcpy(characters[14].code	,"---"	);	characters[14].letter='O';	characters[14].size_of_code=3;
	strcpy(characters[15].code	,".--."	);	characters[15].letter='P';	characters[15].size_of_code=4;
	strcpy(characters[16].code	,"--.-"	);	characters[16].letter='Q';	characters[16].size_of_code=4;
	strcpy(characters[17].code	,".-."	);	characters[17].letter='R';	characters[17].size_of_code=3;
	strcpy(characters[18].code	,"..."	);	characters[18].letter='S';	characters[18].size_of_code=3;
	strcpy(characters[19].code	,"-"	);	characters[19].letter='T';	characters[19].size_of_code=1;
	strcpy(characters[20].code	,"..-"	);	characters[20].letter='U';	characters[20].size_of_code=3;
	strcpy(characters[21].code	,"...-"	);	characters[21].letter='V';	characters[21].size_of_code=4;
	strcpy(characters[22].code	,".--"	);	characters[22].letter='W';	characters[22].size_of_code=3;
	strcpy(characters[23].code	,"-..-"	);	characters[23].letter='X';	characters[23].size_of_code=4;
	strcpy(characters[24].code	,"-.--"	);	characters[24].letter='Y';	characters[24].size_of_code=4;
	strcpy(characters[25].code	,"--.."	);	characters[25].letter='Z';	characters[25].size_of_code=4;
	
	//////////////////////////////////////////////////NUMBERS/////////////////////////////////////////////////////
	strcpy(characters[26].code	,".----");	characters[26].letter='1';	characters[26].size_of_code=5;
	strcpy(characters[27].code	,"..---");	characters[27].letter='2';	characters[27].size_of_code=5;
	strcpy(characters[28].code	,"...--");	characters[28].letter='3';	characters[28].size_of_code=5;
	strcpy(characters[29].code	,"....-");	characters[29].letter='4';	characters[29].size_of_code=5;
	strcpy(characters[30].code	,".....");	characters[30].letter='5';	characters[30].size_of_code=5;
	strcpy(characters[31].code	,"-....");	characters[31].letter='6';	characters[31].size_of_code=5;
	strcpy(characters[32].code	,"--...");	characters[32].letter='7';	characters[32].size_of_code=5;
	strcpy(characters[33].code	,"---..");	characters[33].letter='8';	characters[33].size_of_code=5;
	strcpy(characters[34].code	,"----.");	characters[34].letter='9';	characters[34].size_of_code=5;
	strcpy(characters[35].code	,"-----");	characters[35].letter='0';	characters[35].size_of_code=5;
	
	////////////////////////////////////////////////////SPACE/////////////////////////////////////////////////////
	strcpy(characters[36].code	,".......");	characters[36].letter=' ';	characters[36].size_of_code=6;

	////////////////////////////////////////////////SENTINEL//////////////////////////////////////////////////////
	strcpy(characters[37].code	,"-------");	characters[37].letter='\0';	characters[36].size_of_code=7;

	//END OF INIT FUNCTION
	return;
}

//Function that handles the translation of the input into morse code
void translateCode(char input[], char result[501][8])
{
	int index=0;
	char current;
	codepair currCode;

	fw.write("STARTING TO TRANSLATE MORSE TO ENGLISH", 9);
	//keep reading as long as it isn't the end of line character	
	while(input[index]!='\0' )
	{	
		//traverse the character array to find the matching morse code, then add it to the results array
		current=input[index];
		cout<<current;
		int i;
		for(int i=0; i<MAXCHAR; i++)
		{
			currCode=characters[i];
			if(current==currCode.letter)
			{
				cout<<currCode.code<<" ";
				strcpy(result[index], currCode.code);
		
				break;
			}
		}

		index++;
	}
	//add in the end of line character;
	cout<<"-------"<<endl;
	strcpy(result[index],"-------");
}

//Function that outputs the code stored int result
void outputCode(char result[501][8])
{
	fw.write("STARTING TO OUTPUT", 9);
	initOutputPin(2);
	int index=0;
	int codeIndex;
	//output as long as it isn't the morse code equivalent of the end of line character
	while(strcmp(result[index],"-------" )!=0)
	{
		//iterate through every character in the morse code and then output a dash or a dot accordingly
		codeIndex=0;
		cout<<result[index]<<endl;
		while(result[index][codeIndex]!=0)
		{
			cout<<"Current character: "<<result[index][codeIndex]<<endl;
			if(result[index][codeIndex]=='.')
			{
				//cout<<"Is a dot\n";
				outputDot(2);
			}
			else if(result[index][codeIndex]=='-')
			{
				//cout<<"Is a dash\n";
				outputDash(2);
			}
			codeIndex++;
		}
		//at the end of the character, output the end of character code
		cout<<"ending character"<<endl;
		outputEOC(2);
		index++;
	}
	//since the loop ends before it outputs the end of line character, Manually output the end of line character which is 7 dashes
	for(int i =0 ; i<7; i++)
	{
		outputDash(2);
	}
	//output an end of character to signal the end of the end of file character 
	outputEOC(2);
	freeOutputPin(2);
	return;
}

//Handles what occurs when the user is sending a message
void sendRoutine()
{
	//the input that the user gives us. Used Twitter's character limit as a guidline, we can lower this as needed
	//again, the extra 1 character is for the sentinel value.
	char input[501];
	//get valid input from the user
	validInput(input);
	//store the input that you recieved
	fw.write(input, 9);
	//create an array to store the result of the morse code
	char result[501][8];
	//translate the input into morse code and store in in result
	translateCode(input, result);		
	//output the code that was translated
	outputCode(result);
	return;
}

//What happens when the user is recieveing the code
void recieveRoutine()
{
	fw.write("STARTING TO READ INPUT", 0);
	//INSERT getMorse() HERE, IT WILL STORE THE MORSE CODE INTO THE RESULT ARRAY
		
	char result[501][8];
	bool readMessage = false;
	CODE type;
	int indexResult=0;
	int index=0;
	//while you haven't reached the end of line morse code character, keep recieving	
	while(!readMessage)
	{
		index =0;
		char currChar[8];
		bool done = false;
		//while the end of character code hasn't been sent recieve more characters		
		while(!done)
		{
			//get the type of signal that was sent
			type=inputCheck(3);
			switch(type)
			{
				case err:
					//in this case, something is horribly wrong with the state of the code.
					cerr<<"Invalid code?"<<endl;
					break;
				//if its a dot or a dash, then add it to the currChar array and increment the index
				case dot:
					currChar[index]='.';
					cout<<'.';
					index++;
					break;
				case dash:
					currChar[index]='-';
					cout<<'-';
					index++;
					break;
				//if its the end of character signal, then add the end of line character to the char array and break out of the ineer loop to get the next character
				case eoc:
					currChar[index]='\0';
					cout<<endl;
					done=true;
			}

		}
		//add the code to the result array
		cout<<currChar<<endl;
		strcpy(result[indexResult++],currChar);
		//if the character that was just added is the morse code equivalent of the end of line character, then there is no more input. break out of the loop
		if(!(strcmp("-------", currChar)))
		{
			readMessage=true;
		}
	}
	
	for(int i=0; i<indexResult;i++)
	{
		cout<<result[i]<<endl;
	}

	//Translate the morse code into english
	fw.write("STARTING TO TRANSLATE MORSE TO ENGLISH", 9);	
	index=0;
	codepair currCode;
	//While the morse code isn't the endline character print its english equivalent
	while(strcmp(result[index],"-------" )!=0)
	{
		for(int i=0; i<MAXCHAR; i++)
		{
			currCode=characters[i];
			if(strcmp(result[index],currCode.code)==0)
			{
				cout<<currCode.letter;
			}
		}
		index++;
	} 
	return;
}




int main()
{
	//initialize
	init();
	//Ask if the user is sending or recieving code
	State state = cont();
	//as long as the user isnt done keep running the loop
	while(state!=done)
	{
		//if the user is sending a message, then run the send routine
		if (sending)
		{
			sendRoutine();
		}
		//if the user is receiving a message, then run the receive routine
		else
		{
			recieveRoutine();
		}
		//Figure out the next state of the code
		state = cont();
	}
	//Exit with a nice message
	cout<<"Have a nice day! May Papa Goose be with you."<<endl;
	return 1;
}
