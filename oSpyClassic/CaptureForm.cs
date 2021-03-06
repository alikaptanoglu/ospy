/**
 * Copyright (C) 2006  Ole Andr� Vadla Ravn�s <oleavr@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace oSpyClassic
{
    public partial class CaptureForm : Form
    {
        private AgentListener listener;

        private uint msgCount, msgBytes;
        private uint pktCount, pktBytes;

        private AgentListener.ElementsReceivedHandler receivedHandler;

        public CaptureForm(AgentListener listener, AgentListener.SoftwallRule[] rules)
        {
            InitializeComponent();

            this.listener = listener;
            msgCount = msgBytes = 0;
            pktCount = pktBytes = 0;

            receivedHandler = new AgentListener.ElementsReceivedHandler(listener_MessageElementsReceived);
            listener.MessageElementsReceived += receivedHandler;

            listener.Start(rules);
        }

        private void listener_MessageElementsReceived(AgentListener.MessageQueueElement[] elements)
        {
            if (InvokeRequired)
            {
                Invoke(receivedHandler, new object[] { elements });
                return;
            }

            foreach (AgentListener.MessageQueueElement el in elements)
            {
                if (el.msg_type == MessageType.MESSAGE_TYPE_MESSAGE)
                {
                    msgCount++;
                    msgBytes += (uint) el.message.Length;
                }
                else
                {
                    pktCount++;
                    pktBytes += el.len;
                }
            }

            msgCountLabel.Text = Convert.ToString(msgCount);
            msgBytesLabel.Text = Convert.ToString(msgBytes);
            pktCountLabel.Text = Convert.ToString(pktCount);
            pktBytesLabel.Text = Convert.ToString(pktBytes);
        }

        private void stopButton_Click(object sender, EventArgs e)
        {
            listener.Stop();

            DialogResult = DialogResult.OK;
        }
    }
}