#ifndef HTML_CONTENT_H
#define HTML_CONTENT_H

const char index_html[] PROGMEM = R"rawliteral(

<!doctype html>
<html lang="en">
  <head>
    <title>Slides</title>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <style>
      * {
        box-sizing: border-box;
      }

      /* fill the window with slides and add a gradient background. */
      body {
        min-height: 100%;
        font-family: sans-serif;
        background: #878fec;
        background: radial-gradient(circle, white 0%, #878fec 120%);
        color: white;
        text-align: center;
        padding: 0;
        margin: 0;
        overflow: hidden;
        height: 100vh;
        overflow: hidden;
        font-size: 20px;
        line-height: 1.1;
      }

      .container {
        height: 100%;
        margin: 0 auto;
        display: flex;
        overflow: hidden;
        position: relative;
      }

      /* give each slide the full height and let long texts scroll inside. */
      article {
        overflow-y: auto;
        text-align: center;
        height: 100%;
        display: flex;
        flex-direction: column;
        align-items: center;
        padding: 0 20px;
      }

      /* place the buttons and counter over the slides, without a box. */
      button,
      .message__index {
        padding: 10px;
        position: absolute;
        color: #878fec;
        outline: 0;
        background: none;
        border: none;
        display: block;
      }

      /* keep the index of the current slide in the middle. */
      .message__index {
        width: 100%;
        top: 0;
        text-align: center;
      }

      .message {
        width: 100%;
      }

      /* keep the arrows at the bottom, in the middle. */
      .controls {
        position: absolute;
        bottom: 12px;
        left: 50%;
        transform: translateX(-50%);
        display: flex;
        justify-content: space-between;
      }

      .controls button {
        position: static;
        border: none;
        background: none;
        padding: 10px;
        cursor: pointer;
        font-size: inherit;
        bottom: 0;
        cursor: pointer;
      }

      /* three brown text shadows give the light letters a soft outline */
      .message__content {
        width: 100%;
        text-align: center;
        font-size: 40px;
        font-weight: normal;
        text-shadow:
          0 0 3px #878fec,
          0 0 2px #878fec,
          0 0 5px #878fec;

        margin-top: 20px;
        padding: 10px;
      }

      /* hide messages that are not the current ones */
      .hidden {
        display: none;
      }

      /* ease the next slide in over one second with an animation */
      .fade-in {
        animation: fadeIn 1s ease-in;
      }

      /* defines the animation */
      @keyframes fadeIn {
        from {
          opacity: 0;
        }

        to {
          opacity: 1;
        }
      }
    </style>
  </head>

  <body>
    <div class="container">
      <div id="messageCounter" class="message__index"></div>

      <!-- this is the first slide people see -->
      <!-- the other slides stay hidden until it's their turn -->

      <article class="message">
        <div class="message__content">
          <p>
            Following my wifi spell, you were invited to come here. Look around.
            I am right beside you, a small chip with a battery inside a case of
            recycled plastic.
          </p>
        </div>
      </article>

      <article class="message hidden">
        <div class="message__content">
          <p>
            You didn't connect to the internet this time – you/we/ just became
            y/our own little network. If I am moved, you have to follow my
            signal. If the battery is low, our connection will fade until you
            charge me.
          </p>
        </div>
      </article>

      <article class="message hidden">
        <div class="message__content">
          <p>
            But for now, I am your charming little server, your glittering
            server charm.
          </p>
        </div>
      </article>

      <!-- back and next, at the bottom of the slides -->
      <div class="controls">
        <button id="backButton" type="button">&larr;</button>
        <button id="nextButton" type="button">&rarr;</button>
      </div>
    </div>

    <script>
      // grab the slides, buttons and counter so we can hook them up
      const messages = document.querySelectorAll('.message');
      const backButton = document.getElementById('backButton');
      const nextButton = document.getElementById('nextButton');
      const messageCounter = document.getElementById('messageCounter');

      // start with the first slide; counting in code begins at zero
      let currentMessage = 0;

      // add one so the visible counter starts at 1 instead of 0
      function updateCounter() {
        messageCounter.textContent = `(${currentMessage + 1}/${messages.length})`;
      }

      // hide the old slide, fade in the new one and update the counter
      function showMessage(index) {
        messages[currentMessage].classList.add('hidden');
        currentMessage = index;
        messages[currentMessage].classList.remove('hidden');
        messages[currentMessage].classList.add('fade-in');
        updateCounter();
      }

      updateCounter();

      // going back from the first slide loops around to the last one
      backButton.addEventListener('click', function (event) {
        event.stopPropagation();
        const previousIndex =
          (currentMessage - 1 + messages.length) % messages.length;
        showMessage(previousIndex);
      });

      // after the last slide, start again from the first
      nextButton.addEventListener('click', function (event) {
        event.stopPropagation();
        const nextIndex = (currentMessage + 1) % messages.length;
        showMessage(nextIndex);
      });
    </script>
  </body>
</html>


)rawliteral";

#endif