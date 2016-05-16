#define F (1 << 14)  // fixed point 1
#define INT_MAX ((1<<31)-1)
#define INT_MIN (-(1<<31))
// x and y denote fixed_point numbers in 17.14
// // n is an integer



int int_to_fp (int n); 		 // change  integer to fixed point
int fp_to_int_round (int x);	 // change FP to integer (round off)
int fp_to_int (int x);	 	 // change FP to integer (lose)
int add_fp (int x, int y);	 // add for FP
int add_mixed (int x, int n);	 // add for FP, int
int sub_fp (int x, int y);	 // sub for FP
int sub_mixed (int x, int n);	 // sub for FP, int
int mult_fp (int x, int y);	 // mult for FP
int mult_mixed (int x, int y);   // mult for FP, int
int div_fp (int x, int y); 	 // div for FP
int div_mixed (int x, int n);    // div for FP, int

int int_to_fp (int n){
  return n*F;
}
int fp_to_int_round (int x){
  return x/F;
}   
int fp_to_int (int x){
 if(x>0)
  return (x+F/2)/F;
 else
  return (x-F/2)/F;
// return x/F;
}           
int add_fp (int x, int y){
  return x+y;
}      
int add_mixed (int x, int n){
  return add_fp(x,n*F);
}    
int sub_fp (int x, int y){
  return x-y;
}     
int sub_mixed (int x, int n){
  return x-n*F;
}   
int mult_fp (int x, int y){
  return (int)((int64_t)x*y/F);
}      
int mult_mixed (int x, int y){
  return mult_fp(x,int_to_fp(y));
}  
int div_fp (int x, int y){
  return ((int64_t)x*F/y);
}     
int div_mixed (int x, int n){
  return div_fp(x,int_to_fp(n));
}
