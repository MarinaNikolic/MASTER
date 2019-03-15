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

