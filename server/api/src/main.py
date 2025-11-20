from fastapi import FastAPI
from fastapi.staticfiles import StaticFiles
from fastapi.responses import JSONResponse

app = FastAPI() 

@app.get("/")
def read_root():
    return {"message": "HelloWorld"}

@app.get("/message")
def read_message():
    return {"message": "正常に動作しています"}

# images フォルダを公開（jpgもここに置く）
app.mount("/images", StaticFiles(directory="./src/images"), name="images")

@app.get("/image")
def get_image():
    image_path = [
        "http://10.200.2.7:8080/api/images/calendar.png",
        "http://10.200.2.7:8080/api/images/pikatyu.png"
    ]
    return JSONResponse(content={"images": image_path})