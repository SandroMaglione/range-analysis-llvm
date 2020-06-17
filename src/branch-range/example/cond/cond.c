int fun()
{
    int k = 0;
    int a = 0;

    while (k < 10)
    {
        if (k < 5)
        {
            a = a + 1;
        }
        else
        {
            a = a + 10;
        }
        k = k + 1;
    }

    return a;
}
