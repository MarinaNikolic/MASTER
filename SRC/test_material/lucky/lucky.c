int is_date_incorrect(int date, int month, int year){
	if (month==2 && (date<1 || date >29))
		return 1;
	else if ((month==4 || month==6 || month==9 || month==11) && \
	    		(date<1 || date >30))
		return 1;
	else if ((month==1 || month==3 || month==5 || month==7 || \
			month==8 || month==10 || month==12) \
			&& (date<1 || date >31))
		return 1;
	else if (((year%400)!=0 && ((year%4)!=0 || (year%100)==0)) && \ 
			month==2 && date==29)
		return 1;
	else
		return 0;
}

int to_digit(int value){
	if (value < 10) 
		return value;
	else{
		int sum = 0;
		int base = value;
		int leftover = 0;
		while (base != 0){
			leftover = base % 10;
			sum = sum + leftover;
			base = base / 10;
		}
		return to_digit(sum);
	}
}

int calculate_lucky_number(int day, int month, int year){
	int day_code = to_digit(day);
	int month_code = to_digit(month);
	int year_code = to_digit(year);
	int sum_codes = day_code + month_code + year_code;
	int final_code = to_digit(sum_codes);
	return final_code;
}

