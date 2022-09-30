#ifndef __TUSB546_I2C_H__
#define __TUSB546_I2C_H__

s32 __tusb546_read_reg(const struct i2c_client *client, u8 command);
s32 __tusb546_write_reg(const struct i2c_client *client, u8 command,
		u8 value);
s32 __tusb546_read_block_reg(const struct i2c_client *client, u8 command,
		u8 length, u8 *values);
s32 __tusb546_write_block_reg(const struct i2c_client *client, u8 command,
		u8 length, const u8 *values);

void tusb546_i2c_lock(const struct i2c_client *client);
void tusb546_i2c_unlock(const struct i2c_client *client);

int tusb546_read_reg(struct i2c_client *client, u8 reg);
int tusb546_write_reg(struct i2c_client *client, u8 reg, u8 val);
int tusb546_read_block_reg(struct i2c_client *client,
		u8 reg, u8 len, u8 *data);
int tusb546_write_block_reg(struct i2c_client *client,
		u8 reg, u8 len, const u8 *data);

#endif /* __TUSB546_I2C_H__ */
