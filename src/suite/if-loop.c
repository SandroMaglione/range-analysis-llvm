int fun()
{
    int k = 5;
    int a = 5;

    while (k < 20)
    {
        if (k < 5)
        {
            for (int j = -10; j < 0; ++j)
            {
                a = a + 3;
            }
        }
        else
        {
            for (int j = 10; j > 0; --j)
            {
                a = a - 3;
            }
        }
        k = k + 1;
    }

    return a;
}