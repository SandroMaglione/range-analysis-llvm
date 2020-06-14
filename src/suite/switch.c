int fun()
{
    int k = 0;
    int a = 1;

    while (k < 20)
    {
        switch (k)
        {
        case 1:
            a = a + 1;
            break;
        case 2:
            a = a + 10;
            break;
        default:
            a = a - 1;
            break;
        }
    }

    return a;
}
