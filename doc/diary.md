# Working diary

## 2021-05-22

I am trying to implement 
    
    bool BW_element::EqualKeys(const XB_reader &xb,const XML_Item *src)

This is not so easy. I must compare all key attributes of nodes or even a childrens.

I must provide some size information for `XBT_Compare`.

`XBT_HEX` and `XBT_BLOB` uses 4 bytes size in the first aligned int32. All others hase different sizes.

I like function:

    int BW_element::getData(char **content)

It solves all anomalies in data storage (I am not sure about aligning).

I need getData for comparing. 
