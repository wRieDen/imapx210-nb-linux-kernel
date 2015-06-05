/***
 * This is test a program for an audio codec
 * Copyright (c) 2009~2014 ShangHai Infotm Ltd all rights reserved.
 *
 * Use of Infotm's code is governed by terms and conditions
 * stated in the accompanying licensing statement. 
 *
 * **/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/dirent.h>
//#include <stdio.h>
//#include <math.h>
#include <linux/string.h>

static __init int aplay_init(void)
{
	char musicname[5000] = "aplay",ch;
	char recordname[5000] = "arecord",rec;
	char filename[500];
	char music[100];
	char record[100];
	char *r;
	int l;
	printk("\nPlease input file's path in which you want to play or record:\nfor example ' /udisk4/wav ':");
	//gets(filename);
	/*
	for(l = 0; l < sizeof(filename) && (ch = getchar()) != '\n' ; l ++){
		filename[1] = ch;
	}
	filename[1] = '\0';
	*/
	scanf("%[^\n]",filename);
	/*
	strcat(musicname,filename);
	strcat(musicname,"/");
	printf("%s",musicname);
	*/
	struct dirent **namelist;
	int i,j,m,n,k,h,x,y;
	const char *pext;
	printk("\nYou have choosed file ' %s ' as your file to play or record music.\n",filename);
	printk("\nPlease make your choose to do with file ' %s ':\n 1:Play music;\n 2:Record;\n 3:Exit;\n\n",filename);
	scanf("%d",&m);
	switch(m)
	{
	case 1:
		printk("\nYou have choosed file ' %s ' to play music.\n",filename);
		printk("\nHow many music do you want to play:\n 1:Only one music;\n 2:All the musics;\n 3:exit;\n\n");
		scanf("%d",&i);
		switch(i)
		{
		case 1:
			{
				n = scandir(filename, &namelist, 0, alphasort);
					
				if(n > 2){
					printk("There are %d files in ' %s ',they are:\n",n-2,filename);
					while(n--){
						printk("%s		",namelist[n]->d_name);
					}	
				} else {
					printk("There is no file in ' %s '\n",filename);
					break;
				}

				printk("\nYou have choosed 'Only one music' to play in file ' %s '.\nWhich one do you want to choose:\n 1:Input the music number.\n 2:Input the music name. \n\n", filename);
				
				scanf("%d",&h);
				switch(h)
				{
				case 1:
					printk("\n\nThere are %d files in file ' %s ',which one do you want to play?\nBecareful, if the file you choose is not a .wav music file,it can not be played!\n\n",n-2,filename);
					scanf("%d",&j);
					k = j +1;
					//while(n--)
					pext = strrchr(namelist[k]->d_name,'.');
					if(pext != NULL)
					{
						printk("Music NO.%d's name is ' %s '\n",j,namelist[k]->d_name);
						if(strcmp(pext,".wav") == 0) //*.wav---->play
						{
							//strcat(musicname, " ");  //"aplay *.wav"
							strcat(musicname," ");
							strcat(musicname,"'");
							strcat(musicname,filename);
							strcat(musicname,"'");
							strcat(musicname,"/");
							strcat(musicname,"'");
							strcat(musicname,namelist[k]->d_name);//musicname="aplay *.wav *.wav"
							strcat(musicname,"'");

	                                                printk("Please input the times you want to play:");
							scanf("%d",&x);
							if(x >= 1){
								while(x--){
									system(musicname);
								}
							} else {
								printk("\nSorry,wrong number of times ,play nothing.\n\n");
								break;
							} 

							//system(musicname);
							//strcat(musicname," ");
							//printf("%s",musicname);
							//n=0;
						} else {
							printk("\nSorry,the %dth of ' %s ' is not a .wav music file,please choose another one.\n\n",j,filename);
							break;
						}
					} else {
						 printk("\nSorry,the %dth of ' %s ' is not a .wav music file,please choose another one.\n\n",j,filename);
						 break;
					}
					break;
				case 2:
					//char music[100];
					printk("\nPlease input your music name. Becareful,you should include .wav,or your music will not be played!\n\n");
					//gets(music);
					printk("size of music is %d\n",sizeof(music));
					for(l = 0;l<1000;l++)
					{
						if((ch = getchar()) != '\n'){
							//break;
							music[1] = ch;
						} else {
							break;
							//printf("music is %s",music[1]);
						}	
					}
					printk("music 1 is %s\n",music);
					//music[1]='\0';
					//scanf("%s",&music);
					scanf("%[^\n]",music);
					//scanf("%d",&l);
					strcat(musicname," ");
					strcat(musicname,"'");
					strcat(musicname,filename);
					strcat(musicname,"'");
					strcat(musicname,"/");
					strcat(musicname,"'");
					strcat(musicname,music);
					strcat(musicname,"'");
					system(musicname);

					break;
				}
				break;		
			}
		case 2:
			printk("You have choosed ' Play all the music ' in file ' %s ' .\n",filename);
			n = scandir(filename,&namelist,0,alphasort);

			if(n < 0){
				printk("\nThere is no file in ' %s ',please another file.\n",filename);
				perror("scandir");
			} else {
	                           printk("\nThere are ' %d ' files in ' %s ',\nIf they are all .wav music files, then they are going to be played one by one in range.\nIf there are some non-.wav  files,the  non-.wav files will be passed during the playing .\n\n",n-2,filename);
				   while(n--)
				   {
					pext = strrchr(namelist[n]->d_name,'.');
					if(pext != NULL)
					{
						if(strcmp(pext,".wav") == 0) //*.wav -->play
						{
							strcat(musicname," ");//aplay *.wav
							//strcat(musicname,"/");
							strcat(musicname,"'");
							strcat(musicname,filename);
							strcat(musicname,"'");
							strcat(musicname,"/");
							strcat(musicname,"'");
							strcat(musicname,namelist[n]->d_name);//musicname = "aplay *.wav *.wav"
							strcat(musicname,"'");
							strcat(musicname," ");//aplay *.wav
						} else {
							continue;
						}
					} else {
						printk("namelist->d_name is NULL\n");
						continue;
					}
				   }
				   system(musicname);//system("aplay *.wav *.wav");
			}
			break;
		case 3:
			break;

		}
		break;
	case 2:/*record*/
		n = scandir(filename,&namelist,0,alphasort);
		if(n > 0){
			printk("There are %d files in ' %s ',they are:\n",n-2,filename);
			while(n--){
				printk("%s		",namelist[n]->d_name);
			}
		} else {
			printk("There is no file in  ' %s '\n",filename);
			break;
		}

		printk("\n\nPlease input the record_file's name (such as ' rec_1.wav '),and please remember,\nyou should not create a file which has a same name with any above:\n\n");
		
		for(l = 0;l<1000;l++)
		{
			if((rec = getchar()) != '\n'){
				record[1] = rec;
			} else {
				//printf("record file is %s",record[1]);
				break;
			}
		}
		scanf("%[^\n]",record);
		pext = strrchr(record,'.');
		
		if(pext != NULL){
			if(strcmp(pext,".wav") == 0){
				//printf("\n\nHow long do you want to record:(for example, 100 seconds you should input: 100 )\n\n");
				//scanf("%d",&i);
				//if(i > 0){
					strcat(recordname," ");
					strcat(recordname,"'");
					strcat(recordname,filename);
					strcat(recordname,"'");
					strcat(recordname,"/");
					strcat(recordname,"'");
					strcat(recordname,record);
					strcat(recordname,"'");
					printk("\n\nrecordname is %s\n\n",recordname);
					system(recordname);
			//	} else {
			//		printf("\n\nWrong time!!!\n\n");
			//		break;
			//	}
			} else {
				printk("Wrong file pattern");
				break;
			}
		} else {
			printk("No file pattern!");
			break;
		}
	case 3:/*exit*/
		break;
	}

	return 0;
}
static __exit void aplay_exit(void)
{
	printk("exit");
}

module_init(aplay_init);
module_exit(aplay_exit);

MODULE_DESCRIPTION("test");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zachary");
