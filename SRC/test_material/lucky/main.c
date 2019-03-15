#include <stdio.h>

void greeting(){
	printf("Hello, I am a program that calculates your lucky number\n");
}

void farewell(){
	printf("Goodbye\n");
}

int main(){
	greeting();
	while(1){
		printf("Please, enter y/n to procced/end program\n");
		char c;
		scanf("\n%c", &c);
		if(c == 'n'){
			break;
		}
		else if(c == 'y'){
			int date, month, year;
			printf("Enter your year of birth: ");
			scanf("%d", &year);
			while(year<1900 || year>3000){
				printf("Please enter year between 1900 and 3000: ");
				scanf("%d", &year);
			}
			printf("Enter your month of birth: ");
			scanf("%d", &month);
			while(month<1 || month>12){
				printf("Please enter month between 1 and 12: ");
				scanf("%d", &month);
			}
			printf("Enter your date of birth: ");
			scanf("%d", &date);
			while((month==2 && (date<1 || date >29)) || \
			((month==4 || month==6 || month==9 || month==11) && (date<1 || date >30)) || \
			((month==1 || month==3 || month==5 || month==7 || month==8 || \
			month==10 || month==12) && (date<1 || date >31)) || \
			(((year%400)!=0 && ((year%4)!=0 || (year%100)==0)) && month==2 && date==29)){
				printf("Please enter correct date of %d. month and %d. year: ", month, year);
				scanf("%d", &date);
			}	
			int lucky = calculate_lucky_number(date,month,year);
			printf("Your lucky number is: %d\n",lucky);
		}
	}
	
	farewell();
	return 0;
}
