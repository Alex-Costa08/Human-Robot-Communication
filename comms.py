import serial
import os
import pyaudio
import time
from gtts import gTTS
from google.cloud import dialogflow

def detect_intent_stream(project_id, session_id, language_code):
    """Returns the result of detect intent with streaming audio as input.

    Using the same `session_id` between requests allows continuation
    of the conversation."""

    session_client = dialogflow.SessionsClient()

    # Note: hard coding audio_encoding and sample_rate_hertz for simplicity.
    audio_encoding = dialogflow.AudioEncoding.AUDIO_ENCODING_LINEAR_16
     
    sample_rate_hertz = 44100

    session_path = session_client.session_path(project_id, session_id)
    print("Session path: {}\n".format(session_path))

    def request_generator(audio_config):
        query_input = dialogflow.QueryInput(audio_config=audio_config)

        # The first request contains the configuration.
        yield dialogflow.StreamingDetectIntentRequest(
            session=session_path, query_input=query_input
        )

        # Initialize the microphone input
        audio_input = pyaudio.PyAudio()
        stream = audio_input.open(format=pyaudio.paInt16,
                                 channels=1,
                                 rate=sample_rate_hertz,
                                 input=True,
                                 frames_per_buffer=4096)

        print("Listening for audio...")
        
        start_time = time.time()  # Record the start time
        duration = 3.0  # Set the desired duration in seconds
    
        while time.time() - start_time < duration:  # Continue loop until desired duration is reached
            chunk = stream.read(4096)
            yield dialogflow.StreamingDetectIntentRequest(input_audio=chunk)

        print("Stopped listening.")

        stream.stop_stream()
        stream.close()
        audio_input.terminate()

    audio_config = dialogflow.InputAudioConfig(
        audio_encoding=audio_encoding,
        language_code=language_code,
        sample_rate_hertz=sample_rate_hertz,
    )

    requests = request_generator(audio_config)
    responses = session_client.streaming_detect_intent(requests=requests)

    print("=" * 20)
    for response in responses:
        print(
            'Intermediate transcript: "{}".'.format(
                response.recognition_result.transcript
            )
        )

    # Note: The result from the last response is the final transcript along
    # with the detected content.
    query_result = response.query_result

    print("=" * 20)
    print("Query text: {}".format(query_result.query_text))
    print(
        "Detected intent: {} (confidence: {})\n".format(
            query_result.intent.display_name, query_result.intent_detection_confidence
        )
    )
    print("Fulfillment text: {}\n".format(query_result.fulfillment_text))

    return query_result.query_text, query_result.fulfillment_text, query_result.intent.display_name

def texttospeech(fulfillment, language_code):

    
    # Passing the text and language to the engine, 
    # here we have marked slow=False. Which tells 
    # the module that the converted audio should 
    # have a high speed
    myobj = gTTS(text=fulfillment, lang=language_code, slow=False)
        
    # Saving the converted audio in a mp3 file named
    # welcome 
    myobj.save("welcome.mp3")
  
    # Playing the converted file
    os.system("welcome.mp3")

credentials_path = "key.json"
os.environ['GOOGLE_APPLICATION_CREDENTIALS'] = credentials_path

project_id = "intense-hour-398008"
session_id = "123"
language_code = "en-US"



# Define the serial port and baud rate
ser = serial.Serial('COM9', 115200)  # Replace 'COMX' with your Arduino's port

try:
    while True:
        query, fulfillment, intent = detect_intent_stream(project_id, session_id, language_code)
        print(intent);
        if intent == "Overwhelmed":
            order = "OVERWHELMED,"
            ser.write(order.encode())

        #if query == "hello":
        #    texttospeech(fulfillment, language_code)
        #    order = "90,90,HAPPY,"
        #    ser.write(order.encode())
        #    break
        
        

except KeyboardInterrupt:
    print("\nExiting the program.")

finally:
    ser.close()


