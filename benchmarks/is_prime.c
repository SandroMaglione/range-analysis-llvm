#include<stdio.h>
int main()
{
    int x = 10,i;

    for(i=2;i<=x-1;i++)
        
        if(x%i==0)
            break;
        if(i==x) //that means i value becme eqaul to x thus prime no bcoz every no is divided by itself
            printf("%d is a prime number",x);
        else
            printf("%d is not a prime number",x);
        return 0;

}