#include<stdio.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/ioctl.h>

#define W25_MAGIC       'W'
#define ERASE_NO        0x01
#define W25Q_ERASE      _IO(W25_MAGIC,ERASE_NO)


int main(void)
{
    int file;
    char ch,choice,filename[] = "/dev/w25q64";
    char input[256],output[256];
    unsigned int len;

    file = open(filename,O_RDWR);
    if(file < 0){
	printf("Coudn't open /dev/w25q64\n");
	exit(0);
    }

    do
    {
	printf("Write - w\nRead - r\nErase - e\nChoice = ");
	scanf("%c",&choice);
	getchar();
	
	memset(input,'\0',sizeof(input));
	memset(output,'\0',sizeof(output));

	switch (choice)
	{
	    case 'w':
	    {
		printf("Enter data to store in w25q64 flash (Up to 255 characters)\n");
		scanf("%[^\n]s",input);
		getchar();
		len = strlen(input);

		if(write(file,input,len+1) != len+1)
		{
		    printf("Write fail\n");
		}
		break;
	    }

	    case 'r':
	    {
		read(file,output,255);
		printf("Data stored in w25q64 spi flash\n");
		printf("%s\n",output);
		break;
	    }

	    case 'e':
	    {
	        if(ioctl(file,W25Q_ERASE) != 0)
		{
		    printf("chip Erase Error !!\n");
		}
		printf("chip Erase successfull !!\n");
		break;
	    }
	}

	printf("Press y|Y to read another value = ");	
	scanf("%c",&ch);
	getchar();
    }while(ch == 'y' || ch == 'Y');

    close(file);
    return 0;
}


